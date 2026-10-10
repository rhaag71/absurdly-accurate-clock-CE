#pragma once

#include <cstddef>
#include <cstdint>
#include "clock_state.hpp"
#include "gps_input_event.hpp"

namespace gps_input {
class SimulatedSource {
public:
    void begin() {}
    void start(uint32_t now_us);
    void startAt(uint32_t now_us, int64_t initial_utc_seconds);
    void poll(uint32_t now_us);
    clock_model::Pulse snapshot(uint32_t& now_us) const;
    bool readByte(uint32_t now_us, ByteEvent& event);
    bool pending(uint32_t now_us) const;
    bool startupDrainRequired() const { return false; }
    bool takeDiscontinuity();
    void abortPending();
    bool receiverUtcValid() const { return receiver_utc_valid_; }
    int64_t receiverUtcSeconds() const {
        return receiver_utc_valid_ ? last_receiver_utc_seconds_ : next_utc_seconds_;
    }
    static const char* diagnosticLine() { return "GPS_SOURCE=SIMULATED\r\n"; }

private:
    static constexpr size_t stream_capacity = 192;
    static constexpr uint32_t uart_bits_per_byte = 10;
    bool due(uint32_t now_us, uint32_t deadline_us) const;
    uint32_t byteTime(size_t index) const;
    void buildCycle();
    bool appendSentence(const char* body);

    clock_model::Pulse pulse_;
    clock_model::Pulse stream_pulse_;
    uint32_t next_pps_us_ = 0;
    uint32_t stream_start_us_ = 0;
    uint32_t gga_start_us_ = 0;
    int64_t next_utc_seconds_ = 0;
    int64_t last_receiver_utc_seconds_ = 0;
    char stream_[stream_capacity] = {};
    size_t stream_length_ = 0;
    size_t rmc_length_ = 0;
    size_t stream_offset_ = 0;
    bool started_ = false;
    bool discontinuity_ = false;
    bool receiver_utc_valid_ = false;
};
}
