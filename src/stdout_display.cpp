#include "stdout_display.hpp"

#include <cstdio>
#include "display_time.hpp"

namespace stdout_display {
void Driver::begin(const presentation::State& state) {
    format(state);
    scheduled_ = false;
}

void Driver::format(const presentation::State& state) {
    if (state.clock.utc_valid) {
        const auto local = presentation::convertUtcForDisplay(state.clock.utc, state.zone);
        const int count = std::snprintf(
            line_, sizeof(line_), "%04u-%02u-%02u %02u:%02u:%02u UTC | %s %02u:%02u:%02u | LOCKED\r\n",
            state.clock.utc.year, state.clock.utc.month, state.clock.utc.day,
            state.clock.utc.hour, state.clock.utc.minute, state.clock.utc.second,
            local.label, local.civil.hour,
            local.civil.minute, local.civil.second);
        length_ = count > 0 && static_cast<unsigned>(count) < sizeof(line_)
            ? static_cast<unsigned>(count) : 0;
    } else {
        const char* quality = state.clock.pps_locked ? "QUALIFYING" :
            (state.clock.gps_valid || state.clock.pps_present ? "ACQUIRING" : "UNSYNCED");
        const int count = std::snprintf(
            line_, sizeof(line_), "UTC INVALID | %s | %s\r\n",
            presentation::zoneName(state.zone), quality);
        length_ = count > 0 && static_cast<unsigned>(count) < sizeof(line_)
            ? static_cast<unsigned>(count) : 0;
    }
}

void Driver::service(const presentation::State& state) {
    if (!scheduled_ || static_cast<int32_t>(state.now_us - next_update_us_) >= 0) {
        format(state);
        next_update_us_ = state.now_us + 1000000;
        scheduled_ = true;
    }
    const auto available = sink_.availability(length_);
    if (available == display_output::Availability::quiet) {
        // Drop the pending line; the next enabled pass formats current state.
        length_ = 0;
        scheduled_ = false;
        return;
    }
    if (available == display_output::Availability::blocked || !length_) return;
    if (sink_.write(reinterpret_cast<const uint8_t*>(line_), length_)) length_ = 0;
}
}
