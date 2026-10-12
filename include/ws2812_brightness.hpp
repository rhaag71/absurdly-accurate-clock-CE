#pragma once

#include <cstdint>
#include "display_appearance.hpp"

namespace ws2812_display {
constexpr uint8_t default_brightness = display_appearance::default_clock_brightness;

// Brightness is an output transform: callers retain the full logical color.
inline uint8_t scaleChannel(uint8_t channel, uint8_t brightness) {
    const uint32_t product = static_cast<uint32_t>(channel) * brightness;
    return static_cast<uint8_t>((product + 127u) / 255u);
}

inline uint8_t& brightnessStorage() {
    static uint8_t value = default_brightness;
    return value;
}
inline uint32_t& brightnessGenerationStorage() {
    static uint32_t generation = 0;
    return generation;
}
inline uint8_t brightness() { return brightnessStorage(); }
inline void setBrightness(uint8_t value) {
    if (brightnessStorage() == value) return;
    brightnessStorage() = value;
    ++brightnessGenerationStorage();
}
inline uint32_t brightnessGeneration() { return brightnessGenerationStorage(); }

// Driver-side policy: one changed setting dirties the next complete frame.
class BrightnessRefreshTracker {
public:
    BrightnessRefreshTracker() : observed_(brightnessGeneration()) {}
    void reset() { observed_ = brightnessGeneration(); }
    bool consumeChange() {
        const uint32_t current = brightnessGeneration();
        if (current == observed_) return false;
        observed_ = current;
        return true;
    }
private:
    uint32_t observed_;
};
}
