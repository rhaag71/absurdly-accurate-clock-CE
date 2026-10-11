#pragma once

#include <cstdint>
#include "presentation_state.hpp"
#include "ws2812_pixels.hpp"

namespace ws2812_display {
// The selected driver owns GP8, PIO0 SM0, and its raw-order pixel buffer.
class Driver {
public:
    Driver() = default;
    void begin(const presentation::State& state);
    void service(const presentation::State& state);

private:
    Pixels pixels_;
    uint32_t latch_deadline_us_ = 0;
    uint16_t tx_cursor_ = 0;
    uint8_t pio_sm_ = 0;
    uint8_t program_offset_ = 0;
    uint8_t tx_brightness_ = default_brightness;
    BrightnessRefreshTracker brightness_refresh_;
    char last_text_[6] = {};
    bool initialized_ = false;
    bool tx_active_ = false;
    bool frame_dirty_ = true;
    bool latch_pending_ = false;
    bool has_rendered_text_ = false;

    void updateFrame(const presentation::State& state);
    void sendChunk(uint32_t now_us);
};
}
