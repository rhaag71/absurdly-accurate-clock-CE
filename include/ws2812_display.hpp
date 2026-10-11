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
    enum class DemoStep : uint8_t {
        dark_start, red, green, blue, dark_before_corners,
        corner_top_left, corner_top_right, corner_bottom_right, corner_bottom_left,
        dark_before_perimeter, perimeter, dark_before_rows, rows,
        dark_before_columns, columns, dark_before_chase, chase
    };

    Pixels pixels_;
    uint32_t stage_started_ms_ = 0;
    uint32_t latch_deadline_us_ = 0;
    uint32_t pixel_step_deadline_ms_ = 0;
    uint16_t tx_cursor_ = 0;
    uint16_t pixel_index_ = 0;
    uint8_t pio_sm_ = 0;
    uint8_t program_offset_ = 0;
    DemoStep step_ = DemoStep::dark_start;
    bool initialized_ = false;
    bool tx_active_ = false;
    bool frame_dirty_ = true;
    bool latch_pending_ = false;

    void advanceDemo(uint32_t now_ms);
    void sendChunk(uint32_t now_us);
};
}
