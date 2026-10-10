#include <Arduino.h>
#include <cstring>
#include <cstdio>
#include <hardware/watchdog.h>
#include "watchdog_policy.hpp"
#include "clock_state.hpp"
#include "monotonic_time.hpp"
#include "ce_config.hpp"
#include "gps_input_source.hpp"
#include "display_backend.hpp"
#include "presentation_state.hpp"

namespace {
clock_model::Timebase timebase;
nmea::RmcParser parser;
nmea::GgaParser gga_parser;
gps_input::Selected input;
uint32_t last_heartbeat_ms = 0;
bool heartbeat_on = true;
bool watchdog_boot = false; // Latched once in setup; never cleared on acquisition.
uint32_t last_timebase_poll_us = 0;
bool timebase_poll_seen = false;

void pollTimebase(const clock_model::Pulse& pulse, uint32_t candidate_us) {
    if (!timebase_poll_seen) {
        last_timebase_poll_us = candidate_us;
        timebase_poll_seen = true;
    } else {
        last_timebase_poll_us = clock_model::nondecreasingTimestamp(
            candidate_us, last_timebase_poll_us);
    }
    timebase.poll(pulse, last_timebase_poll_us);
}

// Bounded non-blocking diagnostic queue. Drop complete messages if a host
// remains disconnected through too many transitions; never stall GPS reception.
char usb_queue[1024];
uint32_t diagnostic_drops = 0;
size_t usb_head = 0, usb_tail = 0, usb_used = 0;
void queueDiagnostic(const char* message) {
    const size_t length = std::strlen(message);
    if (length > sizeof(usb_queue) - usb_used) { ++diagnostic_drops; return; }
    for (size_t i = 0; i < length; ++i) {
        usb_queue[usb_head] = message[i];
        usb_head = (usb_head + 1) % sizeof(usb_queue);
    }
    usb_used += length;
}
void diagnostic(const char* message) {
    queueDiagnostic(message);
    static appliance::WatchdogDiagnostic reset_report;
    if (reset_report.next(watchdog_boot)) queueDiagnostic("RESET=WATCHDOG\r\n");
}
void reportTransitions() {
    static bool gps = false, locked = false;
    const auto& state = timebase.state();
    if (state.gps_valid && !gps) diagnostic("RMC UTC data acquired\r\n");
    if (!state.gps_valid && gps) diagnostic("RMC UTC data lost\r\n");
    if (state.pps_locked && !locked) {
        char message[96];
        snprintf(message, sizeof(message),
                 "PPS synchronization acquired; RMC end +%lu ms (preceding PPS)\r\n",
                 static_cast<unsigned long>(state.rmc_phase_us / 1000));
        diagnostic(message);
    }
    if (!state.pps_locked && locked) diagnostic("PPS synchronization lost\r\n");
    gps = state.gps_valid;
    locked = state.pps_locked;
}

display_backend::Selected display;
uint32_t last_service_us = 0;
bool discard_rx = true; // Startup delays buffered bytes without arrival timestamps.
clock_model::Reception reception;

void discardInputAssociation() {
    parser = nmea::RmcParser{};
    gga_parser = nmea::GgaParser{};
    input.abortPending();
    timebase.discardAssociation();
    discard_rx = input.startupDrainRequired();
}

}

void setup() {
    // Read the RP2350 reset cause before arming this boot's watchdog.
    watchdog_boot = watchdog_caused_reboot();
    // Also cover startup stalls.
    // Keep running under debugger halt so a halted main loop can be bench-tested.
    watchdog_enable(appliance::watchdog_timeout_ms, false);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH); // Early indication that setup() was reached.
    last_heartbeat_ms = millis();
    Serial.begin(115200); // USB diagnostics; never wait for a connected host.
    if (watchdog_boot) diagnostic("RESET: watchdog\r\n");
    input.begin();
    const presentation::State initial{
        timebase.state(), {}, micros(), ce_config::civil_timezone};
    display.begin(initial);
    input.start(micros());
    discard_rx = input.startupDrainRequired();
    diagnostic(input.diagnosticLine());
    diagnostic("UTC timebase; RMC labels preceding PPS\r\n");
    last_service_us = micros();

}

void loop() {
    uint32_t now_us = micros();
    input.poll(now_us);
    auto pulse = input.snapshot(now_us);
    // A stalled main loop cannot safely timestamp already-buffered UART bytes.
    if (uint32_t(now_us - last_service_us) > 20000) {
        discardInputAssociation();
        reportTransitions();
    }
    if (input.takeDiscontinuity()) {
        discardInputAssociation();
        reportTransitions();
    }
    last_service_us = now_us;
    pollTimebase(pulse, now_us);
    timebase.satelliteStatus().poll(now_us);
    reportTransitions();

    // Bound work so incoming UART traffic cannot starve pulse/display handling.
    for (unsigned i = 0; i < 64; ++i) {
        gps_input::ByteEvent event;
        if (!input.readByte(micros(), event)) break;
        if (discard_rx) continue;
        now_us = event.timestamp_us;
        pulse = event.pulse;
        pollTimebase(pulse, now_us);
        reportTransitions();
        if (event.byte == '$') {
            reception.sequence = pulse.sequence;
            reception.start_us = now_us;
            reception.usable = pulse.seen;
        }
        nmea::Utc utc;
        char status = '?';
        const auto result = parser.receive(event.byte, utc, status);
        timebase.receive(result, utc, status, reception, now_us);
        uint8_t satellites = 0;
        const auto gga_result = gga_parser.receive(event.byte, satellites);
        timebase.satelliteStatus().receive(gga_result, satellites, now_us);
        reportTransitions();
    }
    if (discard_rx && !input.pending(micros())) discard_rx = false;

    for (unsigned i = 0; i < 64 && usb_used > 0; ++i) {
        if (!Serial || Serial.availableForWrite() <= 0 ||
            Serial.write(static_cast<uint8_t>(usb_queue[usb_tail])) != 1) break;
        usb_tail = (usb_tail + 1) % sizeof(usb_queue);
        --usb_used;
    }
    const uint32_t now = millis();
    if (appliance::heartbeat_due(now, last_heartbeat_ms, watchdog_boot)) {
        last_heartbeat_ms = now;
        heartbeat_on = !heartbeat_on;
        digitalWrite(LED_BUILTIN, heartbeat_on ? HIGH : LOW);
    }
    // Refresh/poll the same pulse snapshot used for visual phase. Neither RMC
    // arrival nor an animation timer can advance the authoritative UTC timebase.
    now_us = micros();
    pulse = input.snapshot(now_us);
    pollTimebase(pulse, now_us);
    timebase.satelliteStatus().poll(now_us);
    reportTransitions();
    const presentation::State shown{
        timebase.state(), pulse, now_us, ce_config::civil_timezone};
    display.service(shown);
    // Sole recurring feed: all main-loop services completed. Degraded inputs,
    // absent peers and display backpressure are valid states, not reset reasons.
    watchdog_update();
}
