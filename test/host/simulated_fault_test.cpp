#include <cassert>
#include <cstdint>
#include <cstring>
#include "ce_config.hpp"
#include "gps_input_simulated.hpp"
#include "nmea_gga.hpp"
#include "nmea_rmc.hpp"

namespace {
struct Observations {
    unsigned rmc_sentences = 0;
    unsigned gga_sentences = 0;
    unsigned valid_rmc = 0;
    unsigned invalid_checksum = 0;
    unsigned invalid_rmc = 0;
    unsigned valid_gga = 0;
    uint32_t first_rmc_start_us = 0;
    uint32_t first_rmc_pulse_us = 0;
    uint32_t first_rmc_completion_us = 0;
    uint32_t last_rmc_label = 0;
    bool have_rmc_start = false;
    bool have_valid_label = false;
    bool labels_consecutive = true;
    bool crossed_midnight = false;
    bool timestamps_monotonic = true;
    nmea::Utc previous_label;
};

class Simulation {
public:
    gps_input::SimulatedSource source;
    clock_model::Timebase timebase;
    nmea::RmcParser rmc;
    nmea::GgaParser gga;
    Observations seen;

    Simulation() { source.start(0); }

    void advanceTo(uint32_t end_us, uint32_t step_us = 500) {
        for (uint32_t at = now_us_ + step_us; at <= end_us; at += step_us) {
            now_us_ = at;
            source.poll(at);
            if (source.takeDiscontinuity()) {
                source.abortPending();
                rmc = nmea::RmcParser{};
                gga = nmea::GgaParser{};
                reception_ = {};
                timebase.discardAssociation();
            }
            uint32_t sampled = at;
            const auto pulse = source.snapshot(sampled);
            timebase.poll(pulse, at);
            timebase.satelliteStatus().poll(at);

            for (;;) {
                gps_input::ByteEvent event;
                if (!source.readByte(at, event)) break;
                observe(event);
                timebase.poll(event.pulse, at);
                if (event.byte == '$') {
                    reception_.sequence = event.pulse.sequence;
                    reception_.start_us = event.timestamp_us;
                    reception_.usable = event.pulse.seen;
                    sentence_used_ = 0;
                    collecting_sentence_ = true;
                }
                if (collecting_sentence_ && sentence_used_ + 1 < sizeof(sentence_)) {
                    sentence_[sentence_used_++] = event.byte;
                    sentence_[sentence_used_] = '\0';
                }

                nmea::Utc utc;
                char status = '?';
                const auto result = rmc.receive(event.byte, utc, status);
                timebase.receive(result, utc, status, reception_, event.timestamp_us);
                if (event.byte == '\r') finishSentence(event, result, utc);

                uint8_t satellites = 0;
                const auto gga_result = gga.receive(event.byte, satellites);
                if (gga_result == nmea::GgaResult::valid_gga) ++seen.valid_gga;
                timebase.satelliteStatus().receive(gga_result, satellites,
                                                   event.timestamp_us);
            }
        }
        now_us_ = end_us;
    }

    uint32_t now() const { return now_us_; }

    void reseed(uint32_t at_us, int64_t epoch) {
        source.abortPending();
        timebase = clock_model::Timebase{};
        rmc = nmea::RmcParser{};
        gga = nmea::GgaParser{};
        reception_ = {};
        seen = {};
        sentence_used_ = 0;
        collecting_sentence_ = false;
        // Keep the prior timer sample so the first post-reseed byte is checked
        // against the still-running monotonic micros() domain.
        now_us_ = at_us;
        source.startAt(at_us, epoch);
    }

private:
    static int hex(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    }

    bool checksumValid() const {
        const char* star = std::strchr(sentence_, '*');
        if (!star || std::strlen(star + 1) < 2) return false;
        const int high = hex(star[1]), low = hex(star[2]);
        if (high < 0 || low < 0) return false;
        uint8_t value = 0;
        for (const char* p = sentence_ + 1; p < star; ++p)
            value ^= static_cast<uint8_t>(*p);
        return value == static_cast<uint8_t>((high << 4) | low);
    }

    void observe(const gps_input::ByteEvent& event) {
        if (last_event_seen_ &&
            static_cast<int32_t>(event.timestamp_us - last_event_us_) < 0)
            seen.timestamps_monotonic = false;
        last_event_us_ = event.timestamp_us;
        last_event_seen_ = true;
    }

    void finishSentence(const gps_input::ByteEvent& event, nmea::Result result,
                        const nmea::Utc& utc) {
        if (!collecting_sentence_) return;
        if (std::strncmp(sentence_, "$GPRMC,", 7) == 0) {
            ++seen.rmc_sentences;
            if (!seen.have_rmc_start) {
                seen.first_rmc_start_us = reception_.start_us;
                seen.first_rmc_pulse_us = event.pulse.at_us;
                seen.first_rmc_completion_us = event.timestamp_us;
                seen.have_rmc_start = true;
            }
            if (!checksumValid()) ++seen.invalid_checksum;
            if (result == nmea::Result::valid_rmc) {
                ++seen.valid_rmc;
                seen.last_rmc_label = static_cast<uint32_t>(clock_model::toUnix(utc));
                if (seen.have_valid_label) {
                    const int64_t previous_epoch = clock_model::toUnix(seen.previous_label);
                    if (clock_model::toUnix(utc) != previous_epoch + 1)
                        seen.labels_consecutive = false;
                    if (seen.previous_label.hour == 23 && seen.previous_label.minute == 59 &&
                        seen.previous_label.second == 59 && utc.hour == 0 &&
                        utc.minute == 0 && utc.second == 0)
                        seen.crossed_midnight = true;
                }
                seen.previous_label = utc;
                seen.have_valid_label = true;
            } else if (result == nmea::Result::invalid_rmc) {
                ++seen.invalid_rmc;
            }
        } else if (std::strncmp(sentence_, "$GPGGA,", 7) == 0) {
            ++seen.gga_sentences;
        }
        collecting_sentence_ = false;
        sentence_used_ = 0;
    }

    uint32_t now_us_ = 0;
    uint32_t last_event_us_ = 0;
    bool last_event_seen_ = false;
    char sentence_[128] = {};
    size_t sentence_used_ = 0;
    bool collecting_sentence_ = false;
    clock_model::Reception reception_;
};

void normalModeStillAcquires() {
    Simulation sim;
    sim.advanceTo(6500000);
    assert(sim.source.faults().gps_messages && sim.source.faults().pps_output);
    assert(sim.source.faults().rmc_mode == gps_input::RmcMode::normal);
    assert(sim.seen.rmc_sentences >= 5 && sim.seen.gga_sentences >= 5);
    assert(sim.seen.invalid_checksum == 0 && sim.seen.valid_rmc >= 5);
    assert(sim.timebase.state().gps_valid && sim.timebase.state().pps_locked &&
           sim.timebase.state().utc_valid);
    assert(sim.timebase.state().satellites.valid);
    assert(sim.seen.timestamps_monotonic);

    const int64_t authority = sim.timebase.state().utc_seconds;
    uint32_t sampled = sim.now();
    const auto pulse_before_clear = sim.source.snapshot(sampled);
    sim.source.setRmcMode(gps_input::RmcMode::bad_checksum);
    sim.source.clearFaults();
    assert(sim.timebase.state().utc_valid && sim.timebase.state().pps_locked);
    assert(sim.timebase.state().utc_seconds == authority);
    sampled = sim.now();
    const auto pulse_after_clear = sim.source.snapshot(sampled);
    assert(pulse_after_clear.sequence == pulse_before_clear.sequence);
    assert(pulse_after_clear.at_us == pulse_before_clear.at_us);
}

void normalLabelsCrossMidnight() {
    Simulation sim;
    nmea::Utc start;
    start.year = 2026;
    start.month = 12;
    start.day = 31;
    start.hour = 23;
    start.minute = 59;
    start.second = 58;
    sim.source.startAt(0, clock_model::toUnix(start));
    sim.advanceTo(6500000);
    assert(sim.seen.valid_rmc >= 5);
    assert(sim.seen.labels_consecutive);
    assert(sim.seen.crossed_midnight);
    assert(sim.timebase.state().utc_valid);
}

void gpsLossExpiresAndResumesCurrentReceiverUtc() {
    Simulation sim;
    sim.advanceTo(6500000);
    assert(sim.timebase.state().utc_valid);
    const uint32_t old_label = sim.seen.last_rmc_label;
    uint32_t sampled = sim.now();
    const uint32_t old_pulse_sequence = sim.source.snapshot(sampled).sequence;
    const unsigned old_rmc_count = sim.seen.rmc_sentences;
    const unsigned old_gga_count = sim.seen.gga_sentences;

    sim.source.setGpsMessages(false);
    sim.advanceTo(9600000);
    sampled = sim.now();
    const auto during_loss = sim.source.snapshot(sampled);
    assert(during_loss.sequence > old_pulse_sequence);
    assert(sim.seen.rmc_sentences == old_rmc_count);
    assert(sim.seen.gga_sentences == old_gga_count);
    assert(!sim.timebase.state().gps_valid && !sim.timebase.state().utc_valid);
    assert(sim.timebase.state().pps_present);

    sim.source.setGpsMessages(true);
    sim.advanceTo(17000000);
    assert(sim.seen.valid_rmc > old_rmc_count);
    assert(sim.seen.last_rmc_label > old_label + 1);
    assert(sim.timebase.state().gps_valid && sim.timebase.state().pps_locked &&
           sim.timebase.state().utc_valid);
    assert(sim.seen.timestamps_monotonic);
}

void ppsLossExpiresAndRequalifiesWithoutCatchup() {
    Simulation sim;
    sim.advanceTo(6500000);
    uint32_t sampled = sim.now();
    const auto before = sim.source.snapshot(sampled);
    const unsigned old_rmc = sim.seen.rmc_sentences;
    const unsigned old_gga = sim.seen.gga_sentences;

    sim.source.setPpsOutput(false);
    sim.advanceTo(9600000);
    sampled = sim.now();
    auto after_off = sim.source.snapshot(sampled);
    assert(after_off.sequence == before.sequence);
    assert(sim.seen.rmc_sentences > old_rmc && sim.seen.gga_sentences > old_gga);
    assert(sim.timebase.state().gps_valid);
    assert(!sim.timebase.state().pps_present && !sim.timebase.state().pps_locked &&
           !sim.timebase.state().utc_valid);

    sim.source.setPpsOutput(true);
    sim.advanceTo(10000000);
    sampled = sim.now();
    auto restored = sim.source.snapshot(sampled);
    assert(restored.sequence == before.sequence + 1);
    assert(restored.at_us == 10000000);
    sim.advanceTo(10500000);
    sampled = sim.now();
    assert(sim.source.snapshot(sampled).sequence == before.sequence + 1);
    sim.advanceTo(11000000);
    sampled = sim.now();
    restored = sim.source.snapshot(sampled);
    assert(restored.sequence == before.sequence + 2 && restored.at_us == 11000000);
    sim.advanceTo(17000000);
    assert(sim.timebase.state().gps_valid && sim.timebase.state().pps_locked &&
           sim.timebase.state().utc_valid);
    assert(sim.seen.timestamps_monotonic);
}

void badChecksumUsesParserAndTimesOutNormally() {
    Simulation sim;
    sim.advanceTo(6500000);
    const unsigned valid_before = sim.seen.valid_rmc;
    const unsigned gga_before = sim.seen.gga_sentences;
    sim.source.setRmcMode(gps_input::RmcMode::bad_checksum);
    sim.advanceTo(10500000);
    assert(sim.seen.rmc_sentences > valid_before);
    assert(sim.seen.invalid_checksum >= 3);
    assert(sim.seen.valid_rmc == valid_before);
    assert(sim.seen.gga_sentences > gga_before);
    assert(!sim.timebase.state().gps_valid && !sim.timebase.state().utc_valid);

    sim.source.setRmcMode(gps_input::RmcMode::normal);
    sim.advanceTo(17000000);
    assert(sim.seen.valid_rmc > valid_before);
    assert(sim.timebase.state().gps_valid && sim.timebase.state().pps_locked &&
           sim.timebase.state().utc_valid);
}

void earlyRmcIsRejectedThenNormalModeRecovers() {
    Simulation sim;
    sim.source.setRmcMode(gps_input::RmcMode::early);
    sim.advanceTo(6500000);
    assert(sim.seen.valid_rmc >= 5);
    const uint32_t early_start = sim.seen.first_rmc_start_us - sim.seen.first_rmc_pulse_us;
    assert(early_start == 6041); // 5 ms schedule plus one 9600-baud byte time.
    assert(early_start < clock_model::Timebase::rmc_min_us);
    assert(!sim.timebase.state().pps_locked && !sim.timebase.state().utc_valid);
    assert(sim.timebase.state().gps_valid); // RMC is valid, only its association is early.

    sim.source.setRmcMode(gps_input::RmcMode::normal);
    sim.advanceTo(13000000);
    assert(sim.timebase.state().gps_valid && sim.timebase.state().pps_locked &&
           sim.timebase.state().utc_valid);
}

void lateRmcCompletesOutsideWindowWithoutGgaOverlap() {
    Simulation sim;
    sim.source.setRmcMode(gps_input::RmcMode::late);
    sim.advanceTo(6500000);
    const uint32_t start = sim.seen.first_rmc_start_us - sim.seen.first_rmc_pulse_us;
    const uint32_t completion = sim.seen.first_rmc_completion_us - sim.seen.first_rmc_pulse_us;
    assert(start == 881041); // 880 ms schedule plus one 9600-baud byte time.
    assert(completion == 916458); // CR completion at 880 ms + 35 wire-byte times.
    assert(completion > clock_model::Timebase::rmc_max_us && completion < 1000000);
    assert(sim.seen.valid_rmc >= 5);
    assert(sim.seen.gga_sentences == 0); // Omitted to keep 1 Hz UART cycles non-overlapping.
    assert(!sim.timebase.state().pps_locked && !sim.timebase.state().utc_valid);
    assert(sim.seen.timestamps_monotonic);

    sim.source.setRmcMode(gps_input::RmcMode::normal);
    sim.advanceTo(13000000);
    assert(sim.seen.gga_sentences > 0);
    assert(sim.timebase.state().gps_valid && sim.timebase.state().pps_locked &&
           sim.timebase.state().utc_valid);
}

void controlsRemainIndependentClearAndReseedPreserveThem() {
    Simulation sim;
    sim.advanceTo(6500000);
    sim.source.setGpsMessages(false);
    sim.source.setPpsOutput(false);
    sim.source.setRmcMode(gps_input::RmcMode::early);
    sim.source.setRmcMode(gps_input::RmcMode::bad_checksum);
    assert(!sim.source.faults().gps_messages && !sim.source.faults().pps_output);
    assert(sim.source.faults().rmc_mode == gps_input::RmcMode::bad_checksum);

    sim.reseed(7000000, 1798187400LL);
    assert(!sim.source.faults().gps_messages && !sim.source.faults().pps_output);
    assert(sim.source.faults().rmc_mode == gps_input::RmcMode::bad_checksum);
    uint32_t sampled = sim.now();
    assert(!sim.source.snapshot(sampled).seen);
    assert(!sim.source.receiverUtcValid());
    sim.advanceTo(10500000);
    assert(sim.seen.rmc_sentences == 0 && sim.seen.gga_sentences == 0);
    sampled = sim.now();
    assert(!sim.source.snapshot(sampled).seen);

    sim.source.setGpsMessages(true);
    sim.advanceTo(13500000);
    assert(sim.seen.rmc_sentences > 0 && sim.seen.invalid_checksum > 0);
    sampled = sim.now();
    assert(!sim.source.snapshot(sampled).seen); // PPS remains independently disabled.

    sim.source.clearFaults();
    assert(sim.source.faults().gps_messages && sim.source.faults().pps_output);
    assert(sim.source.faults().rmc_mode == gps_input::RmcMode::normal);
    assert(!sim.timebase.state().utc_valid); // Clearing receiver controls does not qualify time.
    sim.advanceTo(18500000);
    assert(sim.seen.valid_rmc > 0 && sim.seen.gga_sentences > 0);
    assert(sim.timebase.state().gps_valid && sim.timebase.state().pps_locked &&
           sim.timebase.state().utc_valid);
    assert(sim.seen.last_rmc_label >= 1798187400U);
    assert(sim.seen.timestamps_monotonic);
}
}

int main() {
    normalModeStillAcquires();
    normalLabelsCrossMidnight();
    gpsLossExpiresAndResumesCurrentReceiverUtc();
    ppsLossExpiresAndRequalifiesWithoutCatchup();
    badChecksumUsesParserAndTimesOutNormally();
    earlyRmcIsRejectedThenNormalModeRecovers();
    lateRmcCompletesOutsideWindowWithoutGgaOverlap();
    controlsRemainIndependentClearAndReseedPreserveThem();
}
