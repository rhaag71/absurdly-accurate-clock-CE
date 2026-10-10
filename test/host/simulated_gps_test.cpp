#include <cassert>
#include <cstdint>
#include <type_traits>
#include "ce_config.hpp"
#include "gps_input_source.hpp"

static_assert(std::is_same<gps_input::Selected, gps_input::SimulatedSource>::value,
              "Development uses the simulated source");

namespace {
void clearAssociation(nmea::RmcParser& rmc, nmea::GgaParser& gga,
                      gps_input::Selected& source, clock_model::Timebase& timebase) {
    rmc = nmea::RmcParser{};
    gga = nmea::GgaParser{};
    source.abortPending();
    timebase.discardAssociation();
}

void acquisitionUsesSharedPipeline() {
    gps_input::Selected source;
    clock_model::Timebase timebase;
    nmea::RmcParser rmc;
    nmea::GgaParser gga;
    clock_model::Reception reception;
    uint32_t last_sequence = 0, last_edge_us = 0;
    unsigned edge_count = 0, rmc_count = 0;
    nmea::Utc previous_label;
    bool previous_label_valid = false;
    bool observed_unqualified_start = false;

    assert(!source.startupDrainRequired());
    assert(ce_config::simulated_gps_pps.enabled);
    source.begin();
    source.start(0);

    // No immediate PPS or valid time at simulator startup.
    source.poll(999999);
    uint32_t snapshot_time = 999999;
    auto pulse = source.snapshot(snapshot_time);
    assert(!pulse.seen && !timebase.state().gps_valid &&
           !timebase.state().pps_locked && !timebase.state().utc_valid);

    for (uint32_t now = 1000000; now < 6500000; now += 500) {
        source.poll(now);
        if (source.takeDiscontinuity())
            clearAssociation(rmc, gga, source, timebase);

        snapshot_time = now;
        pulse = source.snapshot(snapshot_time);
        timebase.poll(pulse, now);
        timebase.satelliteStatus().poll(now);

        if (pulse.sequence != last_sequence) {
            ++edge_count;
            if (last_sequence != 0) {
                assert(pulse.sequence - last_sequence == 1);
                assert(pulse.at_us - last_edge_us == 1000000);
            }
            last_sequence = pulse.sequence;
            last_edge_us = pulse.at_us;
        }

        for (;;) {
            gps_input::ByteEvent event;
            if (!source.readByte(now, event)) break;
            const char byte = event.byte;
            const uint32_t arrival_us = event.timestamp_us;
            pulse = event.pulse;
            snapshot_time = arrival_us;
            timebase.poll(pulse, now);
            if (byte == '$') {
                reception.sequence = pulse.sequence;
                reception.start_us = arrival_us;
                reception.usable = pulse.seen;
            }

            nmea::Utc utc;
            char status = '?';
            const auto result = rmc.receive(byte, utc, status);
            if (result == nmea::Result::valid_rmc) {
                assert(status == 'A'); // Parser accepted checksum and valid fix.
                if (previous_label_valid)
                    assert(clock_model::toUnix(utc) ==
                           clock_model::toUnix(previous_label) + 1);
                previous_label = utc;
                previous_label_valid = true;
                ++rmc_count;
                assert(reception.start_us - pulse.at_us >= clock_model::Timebase::rmc_min_us);
                assert(arrival_us - pulse.at_us <= clock_model::Timebase::rmc_max_us);
            }
            timebase.receive(result, utc, status, reception, arrival_us);

            uint8_t satellites = 0;
            const auto gga_result = gga.receive(byte, satellites);
            timebase.satelliteStatus().receive(gga_result, satellites, arrival_us);
        }

        if (pulse.sequence < 4) {
            assert(!timebase.state().pps_locked && !timebase.state().utc_valid);
            observed_unqualified_start = true;
        } else if (pulse.sequence == 4 && timebase.state().utc_valid) {
            assert(clock_model::toUnix(timebase.state().utc) ==
                   ce_config::simulated_gps_pps.initial_utc_seconds + 3);
        }
    }

    assert(observed_unqualified_start);
    assert(edge_count == 6);
    assert(rmc_count >= 5);
    assert(timebase.state().gps_valid && timebase.state().pps_locked &&
           timebase.state().utc_valid);
    // First label is 2026-01-01 00:00:00 on PPS 1. Normal acquisition needs
    // labels on later edges and the following qualified edge to publish UTC.
    assert(clock_model::toUnix(timebase.state().utc) ==
           ce_config::simulated_gps_pps.initial_utc_seconds + 5);
    assert(timebase.state().rmc_phase_us > 200000 &&
           timebase.state().rmc_phase_us < 300000);
    assert(timebase.state().satellites.valid && timebase.state().satellites.used == 8);

    // Drop a later scheduled edge after a loop stall. The following two-second
    // interval must fail the unchanged cadence check and invalidate lock.
    source.poll(7025000);
    assert(source.takeDiscontinuity());
    timebase.discardAssociation();
    snapshot_time = 7025000;
    pulse = source.snapshot(snapshot_time);
    timebase.poll(pulse, 7025000);
    source.poll(8000000);
    snapshot_time = 8000000;
    pulse = source.snapshot(snapshot_time);
    timebase.poll(pulse, 8000000);
    assert(!timebase.state().pps_locked && !timebase.state().utc_valid);
}

void delayedEventsAreNotReplayed() {
    gps_input::Selected source;
    source.start(0);
    source.poll(1000000);
    uint32_t now = 1000000;
    auto pulse = source.snapshot(now);
    assert(pulse.seen && pulse.sequence == 1 && pulse.at_us == 1000000);

    // A 25 ms-late poll drops the due PPS; it does not backdate its timestamp.
    source.poll(2025000);
    now = 2025000;
    pulse = source.snapshot(now);
    assert(pulse.sequence == 1 && pulse.at_us == 1000000);
    assert(source.takeDiscontinuity());

    source.poll(3000000);
    now = 3000000;
    pulse = source.snapshot(now);
    assert(pulse.sequence == 2 && pulse.at_us == 3000000);
}

void monotonicRolloverIsHandled() {
    gps_input::Selected source;
    const uint32_t start = UINT32_MAX - 500000;
    source.start(start);
    const uint32_t first_deadline = start + 1000000;
    source.poll(first_deadline - 1);
    uint32_t now = first_deadline - 1;
    assert(!source.snapshot(now).seen);
    source.poll(first_deadline);
    now = first_deadline;
    auto pulse = source.snapshot(now);
    assert(pulse.seen && pulse.sequence == 1 && pulse.at_us == first_deadline);

    const uint32_t rmc_start = pulse.at_us +
        ce_config::simulated_gps_pps.rmc_start_after_pps_us;
    const uint32_t first_byte_done = rmc_start + 1041;
    source.poll(first_byte_done);
    gps_input::ByteEvent event;
    assert(source.readByte(first_byte_done, event));
    assert(event.byte == '$' && event.timestamp_us == first_byte_done);
    assert(event.pulse.sequence == 1 && event.pulse.at_us == first_deadline);

    const uint32_t second_deadline = first_deadline + 1000000;
    source.poll(second_deadline);
    now = second_deadline;
    pulse = source.snapshot(now);
    assert(pulse.sequence == 2 && pulse.at_us - first_deadline == 1000000);
    // A '$' sampled before the boundary stays paired with its preceding PPS
    // even after the source advances to the next pulse.
    assert(event.byte == '$' && event.timestamp_us < pulse.at_us);
    assert(event.pulse.sequence == 1 && event.pulse.at_us == first_deadline);
}
}

int main() {
    acquisitionUsesSharedPipeline();
    delayedEventsAreNotReplayed();
    monotonicRolloverIsHandled();
}
