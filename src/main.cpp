#include <Arduino.h>
#include <cstring>
#include <cstdio>
#include <hardware/watchdog.h>
#include "watchdog_policy.hpp"
#include "clock_state.hpp"
#include "ce_config.hpp"
#include "display_backend.hpp"
#include "presentation_state.hpp"
#include "pins.hpp"
#include "hardware.hpp"

namespace {
clock_model::Timebase timebase;
nmea::RmcParser parser;
nmea::GgaParser gga_parser;
volatile uint32_t pps_count = 0;
volatile uint32_t pps_at_us = 0;
volatile bool pps_seen = false;
uint32_t last_heartbeat_ms = 0;
bool heartbeat_on = true;
bool watchdog_boot = false; // Latched once in setup; never cleared on acquisition.

void onPpsRise() {
    pps_at_us = micros();
    ++pps_count;
    pps_seen = true;
}
clock_model::Pulse snapshot(uint32_t& now_us) {
    noInterrupts();
    clock_model::Pulse pulse;
    pulse.sequence = pps_count;
    pulse.at_us = pps_at_us;
    pulse.seen = pps_seen;
    now_us = micros(); // Same snapshot: cannot precede the captured edge.
    interrupts();
    return pulse;
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
    if (state.gps_valid && !gps) diagnostic("GPS UTC acquired\r\n");
    if (!state.gps_valid && gps) diagnostic("GPS UTC lost\r\n");
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
    // Retain GPS bytes arriving while display hardware initializes.
    Serial1.setFIFOSize(1024);
    hardware::begin(ce_config::gps_baud);
    attachInterrupt(digitalPinToInterrupt(pins::gps_pps), onPpsRise, RISING);
    const presentation::State initial{
        timebase.state(), {}, micros(), ce_config::civil_timezone};
    display.begin(initial);
    diagnostic("GPS/PPS UTC clock; RMC labels preceding PPS\r\n");
    last_service_us = micros();

}

void loop() {
    uint32_t now_us;
    auto pulse = snapshot(now_us);
    // A stalled main loop cannot safely timestamp already-buffered UART bytes.
    if (uint32_t(now_us - last_service_us) > 20000) {
        discard_rx = true;
        parser = nmea::RmcParser{};
        gga_parser = nmea::GgaParser{};
        timebase.discardAssociation();
        reportTransitions();
    }
    last_service_us = now_us;
    timebase.poll(pulse, now_us);
    timebase.satelliteStatus().poll(now_us);
    reportTransitions();

    // Bound work so incoming UART traffic cannot starve pulse/display handling.
    for (unsigned i = 0; i < 64 && Serial1.available() > 0; ++i) {
        const int received = Serial1.read();
        if (received < 0) break;
        if (discard_rx) continue;
        pulse = snapshot(now_us);
        timebase.poll(pulse, now_us);
        reportTransitions();
        if (received == '$') {
            reception.sequence = pulse.sequence;
            reception.start_us = now_us;
            reception.usable = pulse.seen;
        }
        nmea::Utc utc;
        char status = '?';
        const auto result = parser.receive(static_cast<char>(received), utc, status);
        timebase.receive(result, utc, status, reception, now_us);
        uint8_t satellites = 0;
        const auto gga_result = gga_parser.receive(static_cast<char>(received), satellites);
        timebase.satelliteStatus().receive(gga_result, satellites, now_us);
        reportTransitions();
    }
    if (discard_rx && Serial1.available() == 0) discard_rx = false;

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
    pulse = snapshot(now_us);
    timebase.poll(pulse, now_us);
    timebase.satelliteStatus().poll(now_us);
    reportTransitions();
    const presentation::State shown{
        timebase.state(), pulse, now_us, ce_config::civil_timezone};
    display.service(shown);
    // Sole recurring feed: all main-loop services completed. Degraded inputs,
    // absent peers and display backpressure are valid states, not reset reasons.
    watchdog_update();
}
