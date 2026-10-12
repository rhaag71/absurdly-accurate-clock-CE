#pragma once

#include <cstdint>
#include "display_appearance.hpp"

namespace ws2812_display {
using ClockColor = display_appearance::Rgb;

constexpr ClockColor default_clock_color = display_appearance::default_clock_color;

inline ClockColor& clockColorStorage() {
    static ClockColor value = default_clock_color;
    return value;
}
inline uint32_t& clockColorGenerationStorage() {
    static uint32_t generation = 0;
    return generation;
}
inline ClockColor clockColor() { return clockColorStorage(); }
inline void setClockColor(ClockColor value) {
    const ClockColor old = clockColorStorage();
    if (old.red == value.red && old.green == value.green && old.blue == value.blue) return;
    clockColorStorage() = value;
    ++clockColorGenerationStorage();
}
inline uint32_t clockColorGeneration() { return clockColorGenerationStorage(); }

// A changed logical color requests a complete frame, before output brightness.
class ClockColorRefreshTracker {
public:
    ClockColorRefreshTracker() : observed_(clockColorGeneration()) {}
    void reset() { observed_ = clockColorGeneration(); }
    bool consumeChange() {
        const uint32_t current = clockColorGeneration();
        if (current == observed_) return false;
        observed_ = current;
        return true;
    }
private:
    uint32_t observed_;
};
}
