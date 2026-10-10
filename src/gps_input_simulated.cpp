#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
#include "gps_input_simulated.hpp"
#include <cstdio>
#include <cstring>
#include "ce_config.hpp"

namespace gps_input {
namespace {
constexpr uint32_t bits_per_second = 10000000; // 10 serial bits per byte.
constexpr uint32_t inter_sentence_gap_us = 50000;

bool appendNmea(char* output, size_t capacity, size_t& used, const char* body) {
    uint8_t checksum = 0;
    for (const char* cursor = body; *cursor; ++cursor)
        checksum ^= static_cast<uint8_t>(*cursor);
    const int count = std::snprintf(output + used, capacity - used,
                                    "$%s*%02X\r\n", body, checksum);
    if (count < 0 || static_cast<size_t>(count) >= capacity - used) return false;
    used += static_cast<size_t>(count);
    return true;
}
}

bool SimulatedSource::due(uint32_t now_us, uint32_t deadline_us) const {
    return static_cast<int32_t>(now_us - deadline_us) >= 0;
}

void SimulatedSource::start(uint32_t now_us) {
    startAt(now_us, ce_config::simulated_gps_pps.initial_utc_seconds);
}

void SimulatedSource::startAt(uint32_t now_us, int64_t initial_utc_seconds) {
    pulse_ = {};
    stream_pulse_ = {};
    stream_length_ = rmc_length_ = stream_offset_ = 0;
    discontinuity_ = false;
    receiver_utc_valid_ = false;
    next_utc_seconds_ = initial_utc_seconds;
    next_pps_us_ = now_us + ce_config::simulated_gps_pps.pps_period_us;
    started_ = ce_config::simulated_gps_pps.enabled;
}

uint32_t SimulatedSource::byteTime(size_t index) const {
    const uint32_t baud = static_cast<uint32_t>(ce_config::gps_baud);
    if (index < rmc_length_) {
        return stream_start_us_ + static_cast<uint32_t>(
            (static_cast<uint64_t>(index + 1) * bits_per_second) / baud);
    }
    return gga_start_us_ + static_cast<uint32_t>(
        (static_cast<uint64_t>(index - rmc_length_ + 1) * bits_per_second) / baud);
}

bool SimulatedSource::appendSentence(const char* body) {
    return appendNmea(stream_, sizeof(stream_), stream_length_, body);
}

void SimulatedSource::buildCycle() {
    stream_length_ = rmc_length_ = stream_offset_ = 0;
    const auto utc = clock_model::fromUnix(next_utc_seconds_);
    char body[96];
    int count = std::snprintf(body, sizeof(body),
        "GPRMC,%02u%02u%02u.00,A,,,,,,,%02u%02u%02u",
        utc.hour, utc.minute, utc.second, utc.day, utc.month,
        static_cast<unsigned>(utc.year % 100));
    if (count < 0 || static_cast<size_t>(count) >= sizeof(body) ||
        !appendSentence(body)) {
        stream_length_ = rmc_length_ = 0;
        discontinuity_ = true;
        return;
    }
    rmc_length_ = stream_length_;

    count = std::snprintf(body, sizeof(body),
        "GPGGA,%02u%02u%02u.00,0000.0000,N,00000.0000,E,1,08,1.0,0.0,M,0.0,M,,",
        utc.hour, utc.minute, utc.second);
    if (count < 0 || static_cast<size_t>(count) >= sizeof(body) ||
        !appendSentence(body)) {
        stream_length_ = rmc_length_ = 0;
        discontinuity_ = true;
        return;
    }

    stream_start_us_ = pulse_.at_us + ce_config::simulated_gps_pps.rmc_start_after_pps_us;
    stream_pulse_ = pulse_;
    last_receiver_utc_seconds_ = next_utc_seconds_;
    receiver_utc_valid_ = true;
    const uint32_t rmc_wire_time = static_cast<uint32_t>(
        (static_cast<uint64_t>(rmc_length_) * bits_per_second) / ce_config::gps_baud);
    gga_start_us_ = stream_start_us_ + rmc_wire_time + inter_sentence_gap_us;
    ++next_utc_seconds_;
}

void SimulatedSource::poll(uint32_t now_us) {
    if (!started_) return;

    if (stream_offset_ < stream_length_) {
        const uint32_t next_byte_us = byteTime(stream_offset_);
        if (due(now_us, next_byte_us) &&
            static_cast<uint32_t>(now_us - next_byte_us) >
                ce_config::simulated_gps_pps.maximum_event_lateness_us) {
            abortPending();
            discontinuity_ = true;
        }
    }

    if (!due(now_us, next_pps_us_)) return;
    const uint32_t lateness = now_us - next_pps_us_;
    const uint32_t period = ce_config::simulated_gps_pps.pps_period_us;
    if (lateness > ce_config::simulated_gps_pps.maximum_event_lateness_us) {
        abortPending();
        discontinuity_ = true;
        const uint32_t missed_periods = lateness / period + 1;
        next_pps_us_ += missed_periods * period;
        next_utc_seconds_ += missed_periods;
        return;
    }

    // Timestamp the event when the source actually produces it. Never replay
    // a missed edge at its old scheduled timestamp.
    pulse_.at_us = now_us;
    pulse_.seen = true;
    ++pulse_.sequence;
    buildCycle();
    next_pps_us_ += period;
}

clock_model::Pulse SimulatedSource::snapshot(uint32_t&) const {
    return pulse_;
}

bool SimulatedSource::readByte(uint32_t now_us, ByteEvent& event) {
    if (stream_offset_ >= stream_length_) return false;
    const uint32_t next_byte_us = byteTime(stream_offset_);
    if (!due(now_us, next_byte_us)) return false;
    if (static_cast<uint32_t>(now_us - next_byte_us) >
        ce_config::simulated_gps_pps.maximum_event_lateness_us) {
        abortPending();
        discontinuity_ = true;
        return false;
    }
    event.timestamp_us = next_byte_us;
    event.pulse = stream_pulse_;
    event.byte = stream_[stream_offset_++];
    if (stream_offset_ == stream_length_)
        stream_length_ = rmc_length_ = stream_offset_ = 0;
    return true;
}

bool SimulatedSource::pending(uint32_t now_us) const {
    return stream_offset_ < stream_length_ && due(now_us, byteTime(stream_offset_));
}

bool SimulatedSource::takeDiscontinuity() {
    const bool result = discontinuity_;
    discontinuity_ = false;
    return result;
}

void SimulatedSource::abortPending() {
    stream_length_ = rmc_length_ = stream_offset_ = 0;
}
}
#endif
