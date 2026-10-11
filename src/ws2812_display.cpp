#include "ws2812_display.hpp"

#if defined(AAC_DISPLAY_WS2812)
#include <Arduino.h>
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "pins.hpp"

namespace ws2812_display {
namespace {
constexpr uint8_t bringup_level = 8;
constexpr uint32_t fill_hold_ms = 500;
constexpr uint32_t corner_hold_ms = 1800;
constexpr uint32_t chase_step_ms = 35;
constexpr uint32_t perimeter_step_ms = 35;
constexpr uint32_t row_hold_ms = 500;
constexpr uint32_t column_hold_ms = 150;

// With the configured 150 MHz system clock and divider 15, PIO runs at
// 10 MHz. Ten PIO cycles encode each 800 kHz WS2812 bit: T0H/T0L = 3/7
// cycles and T1H/T1L = 6/4 cycles. The post-frame low interval is handled
// nonblockingly by Driver::service().
uint16_t pio_instructions[] = {
    static_cast<uint16_t>(pio_encode_out(pio_x, 1) | pio_encode_sideset(1, 0) |
                          pio_encode_delay(3)),
    static_cast<uint16_t>(pio_encode_jmp_not_x(3) | pio_encode_sideset(1, 1) |
                          pio_encode_delay(2)),
    static_cast<uint16_t>(pio_encode_jmp(0) | pio_encode_sideset(1, 1) |
                          pio_encode_delay(2)),
    static_cast<uint16_t>(pio_encode_nop() | pio_encode_sideset(1, 0) |
                          pio_encode_delay(2)),
};
const pio_program ws2812_program = {pio_instructions, 4, 0, 0, 1};

bool deadlineReached(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
}
}

void Driver::begin(const presentation::State&) {
    // Hold the HCT541 input low before handing GP8 to PIO.
    gpio_init(pins::ws2812_data);
    gpio_put(pins::ws2812_data, false);
    gpio_set_dir(pins::ws2812_data, GPIO_OUT);

    if (!pio_can_add_program(pio0, &ws2812_program)) return;
    program_offset_ = static_cast<uint8_t>(pio_add_program(pio0, &ws2812_program));
    if (program_offset_ != 0 || pio_sm_is_claimed(pio0, 0)) {
        pio_remove_program(pio0, &ws2812_program, program_offset_);
        return;
    }

    pio_sm_ = 0;
    pio_sm_claim(pio0, pio_sm_);
    pio_gpio_init(pio0, pins::ws2812_data);

    pio_sm_config config = pio_get_default_sm_config();
    sm_config_set_wrap(&config, program_offset_, program_offset_ + 3);
    sm_config_set_sideset(&config, 1, false, false);
    sm_config_set_sideset_pins(&config, pins::ws2812_data);
    sm_config_set_out_pins(&config, pins::ws2812_data, 1);
    sm_config_set_out_shift(&config, false, true, 24);
    sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_TX);
    sm_config_set_clkdiv(&config, 15.0f);
    pio_sm_init(pio0, pio_sm_, program_offset_, &config);
    pio_sm_set_pins_with_mask(pio0, pio_sm_, 0, 1u << pins::ws2812_data);
    pio_sm_set_consecutive_pindirs(pio0, pio_sm_, pins::ws2812_data, 1, true);
    pio_sm_set_enabled(pio0, pio_sm_, true);

    pixels_.clear();
    step_ = DemoStep::dark_start;
    stage_started_ms_ = millis();
    frame_dirty_ = true; // The first frame is completely dark.
    initialized_ = true;
}

void Driver::advanceDemo(uint32_t now_ms) {
    if (step_ == DemoStep::perimeter) {
        if (!deadlineReached(now_ms, pixel_step_deadline_ms_)) return;
        if (pixel_index_ + 1 >= perimeter_pixel_count) {
            step_ = DemoStep::dark_before_rows;
            stage_started_ms_ = now_ms;
            pixels_.clear();
            frame_dirty_ = true;
            return;
        }
        pixel_step_deadline_ms_ = now_ms + perimeter_step_ms;
        ++pixel_index_;
        int x = 0;
        int y = 0;
        pixels_.clear();
        if (perimeterStepToXY(pixel_index_, x, y))
            pixels_.setLogical(x, y, {255, 255, 255});
        frame_dirty_ = true;
        return;
    }

    if (step_ == DemoStep::rows || step_ == DemoStep::columns) {
        const bool scanning_rows = step_ == DemoStep::rows;
        const uint32_t hold_ms = scanning_rows ? row_hold_ms : column_hold_ms;
        if (static_cast<uint32_t>(now_ms - stage_started_ms_) < hold_ms) return;
        stage_started_ms_ = now_ms;
        const uint16_t limit = scanning_rows ? logical_height : logical_width;
        if (pixel_index_ + 1 >= limit) {
            step_ = scanning_rows ? DemoStep::dark_before_columns
                                  : DemoStep::dark_before_chase;
            pixels_.clear();
        } else {
            ++pixel_index_;
            if (scanning_rows)
                setLogicalRowFrame(pixels_, pixel_index_, {255, 255, 255});
            else
                setLogicalColumnFrame(pixels_, pixel_index_, {255, 255, 255});
        }
        frame_dirty_ = true;
        return;
    }

    if (step_ == DemoStep::chase) {
        if (!deadlineReached(now_ms, pixel_step_deadline_ms_)) return;
        pixel_step_deadline_ms_ = now_ms + chase_step_ms;
        pixels_.clear();
        pixel_index_ = static_cast<uint16_t>((pixel_index_ + 1) % pixel_count);
        pixels_.set(pixel_index_, {255, 255, 255});
        frame_dirty_ = true;
        return;
    }

    const bool showing_corner = step_ == DemoStep::corner_top_left ||
                                step_ == DemoStep::corner_top_right ||
                                step_ == DemoStep::corner_bottom_right ||
                                step_ == DemoStep::corner_bottom_left;
    const uint32_t hold_ms = showing_corner ? corner_hold_ms : fill_hold_ms;
    if (static_cast<uint32_t>(now_ms - stage_started_ms_) < hold_ms) return;
    stage_started_ms_ = now_ms;
    switch (step_) {
    case DemoStep::dark_start:
        step_ = DemoStep::red;
        pixels_.fill({255, 0, 0});
        break;
    case DemoStep::red:
        step_ = DemoStep::green;
        pixels_.fill({0, 255, 0});
        break;
    case DemoStep::green:
        step_ = DemoStep::blue;
        pixels_.fill({0, 0, 255});
        break;
    case DemoStep::blue:
        step_ = DemoStep::dark_before_corners;
        pixels_.clear();
        break;
    case DemoStep::dark_before_corners:
        step_ = DemoStep::corner_top_left;
        pixels_.clear();
        pixels_.setLogical(0, 0, {255, 0, 0});
        break;
    case DemoStep::corner_top_left:
        step_ = DemoStep::corner_top_right;
        pixels_.clear();
        pixels_.setLogical(31, 0, {0, 255, 0});
        break;
    case DemoStep::corner_top_right:
        step_ = DemoStep::corner_bottom_right;
        pixels_.clear();
        pixels_.setLogical(31, 7, {0, 0, 255});
        break;
    case DemoStep::corner_bottom_right:
        step_ = DemoStep::corner_bottom_left;
        pixels_.clear();
        pixels_.setLogical(0, 7, {255, 255, 0});
        break;
    case DemoStep::corner_bottom_left:
        step_ = DemoStep::dark_before_perimeter;
        pixels_.clear();
        break;
    case DemoStep::dark_before_perimeter: {
        step_ = DemoStep::perimeter;
        pixel_index_ = 0;
        pixel_step_deadline_ms_ = now_ms + perimeter_step_ms;
        int x = 0;
        int y = 0;
        pixels_.clear();
        if (perimeterStepToXY(pixel_index_, x, y))
            pixels_.setLogical(x, y, {255, 255, 255});
        break;
    }
    case DemoStep::perimeter:
        return;
    case DemoStep::dark_before_rows:
        step_ = DemoStep::rows;
        pixel_index_ = 0;
        setLogicalRowFrame(pixels_, pixel_index_, {255, 255, 255});
        break;
    case DemoStep::rows:
        return;
    case DemoStep::dark_before_columns:
        step_ = DemoStep::columns;
        pixel_index_ = 0;
        setLogicalColumnFrame(pixels_, pixel_index_, {255, 255, 255});
        break;
    case DemoStep::columns:
        return;
    case DemoStep::dark_before_chase:
        step_ = DemoStep::chase;
        pixel_index_ = 0;
        pixel_step_deadline_ms_ = now_ms + chase_step_ms;
        pixels_.set(pixel_index_, {255, 255, 255});
        break;
    case DemoStep::chase:
        return;
    }
    frame_dirty_ = true;
}

void Driver::sendChunk(uint32_t now_us) {
    unsigned sent = 0;
    while (sent < 8 && tx_cursor_ < pixel_count &&
           !pio_sm_is_tx_fifo_full(pio0, pio_sm_)) {
        pio_sm_put(pio0, pio_sm_, pixels_.grbWord(tx_cursor_++, bringup_level));
        ++sent;
    }
    if (tx_cursor_ == pixel_count) {
        tx_active_ = false;
        latch_pending_ = true;
        latch_deadline_us_ = now_us + 1000;
    }
}

void Driver::service(const presentation::State&) {
    if (!initialized_) return;

    const uint32_t now_us = micros();
    const uint32_t now_ms = millis();
    if (tx_active_) {
        sendChunk(now_us);
        return;
    }
    if (latch_pending_) {
        if (!deadlineReached(now_us, latch_deadline_us_)) return;
        latch_pending_ = false;
    }

#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
    if (!frame_dirty_) advanceDemo(now_ms);
#endif

    if (frame_dirty_) {
        tx_cursor_ = 0;
        tx_active_ = true;
        frame_dirty_ = false;
        sendChunk(now_us);
    }
}
}
#endif
