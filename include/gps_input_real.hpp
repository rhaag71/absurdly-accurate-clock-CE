#pragma once

#include <cstdint>
#include "clock_state.hpp"
#include "gps_input_event.hpp"

namespace gps_input {
class RealSource {
public:
    void begin();
    void start(uint32_t now_us);
    void poll(uint32_t now_us);
    clock_model::Pulse snapshot(uint32_t& now_us) const;
    bool readByte(uint32_t now_us, ByteEvent& event);
    bool pending(uint32_t now_us) const;
    bool startupDrainRequired() const { return true; }
    bool takeDiscontinuity() { return false; }
    void abortPending() {}
    void capturePps(uint32_t at_us);
    static const char* diagnosticLine() { return "GPS_SOURCE=REAL\r\n"; }
private:
    volatile uint32_t pps_count_ = 0;
    volatile uint32_t pps_at_us_ = 0;
    volatile bool pps_seen_ = false;
};
}
