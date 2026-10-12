#include "ws2812_display.hpp"

#if defined(AAC_DISPLAY_WS2812)
#include <Arduino.h>
#include <cstring>
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "clock_text_renderer.hpp"
#include "display_compositor.hpp"
#include "pins.hpp"

namespace ws2812_display {
namespace {
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
    frame_dirty_ = true; // The first frame is completely dark.
    has_rendered_text_ = false;
    brightness_refresh_.reset();
    color_refresh_.reset();
    transfer_refresh_.reset();
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
    proof_mode_refresh_.reset();
    test_pattern_refresh_.reset();
    last_frame_was_pattern_ = false;
#endif
    initialized_ = true;
}

void Driver::updateFrame(const presentation::State& state, bool force_refresh) {
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
    const TestPattern pattern = testPattern();
    if (pattern != TestPattern::clock) {
        if (!last_frame_was_pattern_ || force_refresh) {
            renderTestPattern(pixels_, pattern);
            frame_dirty_ = true;
            last_frame_was_pattern_ = true;
        }
        return;
    }
#endif
    char text[6] = {};
    const bool valid = clock_text_renderer::formatHHMM(state, text);
    if (valid && has_rendered_text_ && std::memcmp(last_text_, text, sizeof(text)) == 0 &&
        !force_refresh)
        return;
    if (!valid && !has_rendered_text_) {
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
        if (last_frame_was_pattern_) {
            pixels_.clear();
            frame_dirty_ = true;
            last_frame_was_pattern_ = false;
        }
#endif
        return;
    }

    if (valid) {
        const display_appearance::Rgb color =
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
            proofClockColor(proofMode(), clockColor());
#else
            clockColor();
#endif
        const display_appearance::Rgb background =
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
            proofBackground(proofMode());
#else
            {0, 0, 0};
#endif
        display_compositor::composeClock(
            pixels_, text, logical_width, 0, background,
            display_compositor::solid(color));
        std::memcpy(last_text_, text, sizeof(last_text_));
        has_rendered_text_ = true;
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
        last_frame_was_pattern_ = false;
#endif
    } else {
        // A newly invalid authoritative snapshot clears the old time once.
        pixels_.clear();
        last_text_[0] = 0;
        has_rendered_text_ = false;
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
        last_frame_was_pattern_ = false;
#endif
    }
    frame_dirty_ = true;
}

void Driver::sendChunk(uint32_t now_us) {
    unsigned sent = 0;
    while (sent < 8 && tx_cursor_ < pixel_count &&
           !pio_sm_is_tx_fifo_full(pio0, pio_sm_)) {
        pio_sm_put(pio0, pio_sm_, pixels_.grbWord(
            tx_cursor_++, tx_brightness_, tx_transfer_));
        ++sent;
    }
    if (tx_cursor_ == pixel_count) {
        tx_active_ = false;
        latch_pending_ = true;
        latch_deadline_us_ = now_us + 1000;
    }
}

void Driver::service(const presentation::State& state) {
    if (!initialized_) return;

    const uint32_t now_us = micros();
    if (tx_active_) {
        sendChunk(now_us);
        return;
    }
    if (latch_pending_) {
        if (!deadlineReached(now_us, latch_deadline_us_)) return;
        latch_pending_ = false;
    }

    const bool brightness_changed = brightness_refresh_.consumeChange();
    const bool color_changed = color_refresh_.consumeChange();
    const bool transfer_changed = transfer_refresh_.consumeChange();
    bool force_refresh = brightness_changed || color_changed || transfer_changed;
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
    const bool proof_mode_changed = proof_mode_refresh_.consumeChange();
    const bool pattern_changed = test_pattern_refresh_.consumeChange();
    force_refresh = force_refresh || pattern_changed || proof_mode_changed;
#endif
    if (force_refresh) frame_dirty_ = true;

    updateFrame(state, force_refresh);

    if (frame_dirty_) {
        tx_cursor_ = 0;
        tx_brightness_ = brightness();
        // Keep every pixel in one transmitted frame on the same transfer
        // mode, even if a development command arrives during chunked output.
        tx_transfer_ = display_appearance::transferMode();
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
        tx_brightness_ = testPatternBrightness(testPattern(), brightness());
        if (testPattern() == TestPattern::clock)
            tx_brightness_ = proofBrightness(proofMode(), brightness());
#endif
        tx_active_ = true;
        frame_dirty_ = false;
        sendChunk(now_us);
    }
}
}
#endif
