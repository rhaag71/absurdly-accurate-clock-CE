#include "ws2812_display.hpp"

#if defined(AAC_DISPLAY_WS2812)
#include <Arduino.h>
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/sync.h"
#include "hardware/pio.h"
#include "clock_text_renderer.hpp"
#include "display_compositor.hpp"
#include "pins.hpp"

namespace ws2812_display {
namespace {
// Ten PIO cycles per bit at 8 MHz give the intended 800 kbit/s stream.
// DREQ-paced DMA prevents main-loop/USB delays from starving the TX FIFO.
// FIFO/OSR drain and reset-low settling are polled, never waited on here.
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
    dma_channel_ = dma_claim_unused_channel(false);
    if (dma_channel_ < 0) {
        // Optional display failure leaves timekeeping and watchdog independent.
        pio_sm_unclaim(pio0, pio_sm_);
        pio_remove_program(pio0, &ws2812_program, program_offset_);
        return;
    }
    pio_gpio_init(pio0, pins::ws2812_data);

    pio_sm_config config = pio_get_default_sm_config();
    sm_config_set_wrap(&config, program_offset_, program_offset_ + 3);
    sm_config_set_sideset(&config, 1, false, false);
    sm_config_set_sideset_pins(&config, pins::ws2812_data);
    sm_config_set_out_pins(&config, pins::ws2812_data, 1);
    sm_config_set_out_shift(&config, false, true, 24);
    sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_TX);
    sm_config_set_clkdiv(&config, static_cast<float>(clock_get_hz(clk_sys)) /
        (wire_bit_rate * wire_cycles_per_bit));
    pio_sm_init(pio0, pio_sm_, program_offset_, &config);
    pio_sm_set_pins_with_mask(pio0, pio_sm_, 0, 1u << pins::ws2812_data);
    pio_sm_set_consecutive_pindirs(pio0, pio_sm_, pins::ws2812_data, 1, true);
    pio_sm_set_enabled(pio0, pio_sm_, true);

    pixels_.clear();
    frame_dirty_ = true; // The first frame is completely dark.
    clock_transition_.reset();
    brightness_refresh_.reset();
    color_refresh_.reset();
    transfer_refresh_.reset();
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
    temporal_dither_.reset();
    temporalDitherRefreshSchedule().stop();
    active_dither_mode_ = TemporalDitherMode::off;
    effect_dither_requested_ = false;
    proof_mode_refresh_.reset();
    test_pattern_refresh_.reset();
    last_frame_was_pattern_ = false;
#else
    temporal_dither_.reset();
    effect_dither_schedule_.stop();
    active_dither_mode_ = TemporalDitherMode::off;
    effect_dither_requested_ = false;
#endif
    overlay_frame_initialized_ = false;
    holdover_was_active_ = false;
    last_holdover_redraw_us_ = 0;
    initialized_ = true;
}

void Driver::updateFrame(const presentation::State& state, bool force_refresh,
                         const informational_overlay::Frame& overlay) {
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
    const TestPattern pattern = testPattern();
    if (pattern != TestPattern::clock) {
        clock_transition_.reset();
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
    int centered_x = 0;
    if (valid) {
        const int rendered_width = compact_clock_font::textWidth(text);
        if (rendered_width < 0 || rendered_width > logical_width) return;
        centered_x = (logical_width - rendered_width) / 2;
    }
    uint8_t civil_hour = 0U;
    uint8_t civil_minute = 0U;
    if (valid) {
        const presentation::DisplayTime local =
            presentation::convertUtcForDisplay(state.clock.utc, state.zone);
        civil_hour = local.civil.hour;
        civil_minute = local.civil.minute;
    }
    const clock_presentation_transition::UpdateResult transition =
        clock_transition_.update(valid, civil_hour, civil_minute, text,
                                 centered_x, state.now_us);
    if (!force_refresh && !transition.changed) return;

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
    clock_transition_.render(pixels_, background, color);
    // Persistent state/date pixels are painted after every proof background
    // and clock mask so inversion and future decoration cannot cover them.
    informational_overlay::apply(pixels_, overlay);
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
    last_frame_was_pattern_ = false;
#endif
    frame_dirty_ = true;
}

void Driver::service(const presentation::State& state) {
    if (!initialized_) return;

#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
    TemporalDitherRefreshSchedule& dither_schedule =
        temporalDitherRefreshSchedule();
#else
    TemporalDitherRefreshSchedule& dither_schedule = effect_dither_schedule_;
#endif

    // Neither compositor nor frame preparation can run until the DMA stream,
    // last FIFO/OSR word, and reset-low interval have all completed.
    if (wire_frame_.busy()) {
        const bool dma_busy = dma_channel_is_busy(dma_channel_);
        // Timestamp after observing DMA completion, so a preempting interrupt
        // cannot shorten the conservative FIFO-drain/reset interval.
        const uint32_t poll_now_us = micros();
        if (!wire_frame_.poll(poll_now_us, dma_busy)) {
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
            if (active_dither_mode_ != TemporalDitherMode::off)
                temporalDitherRefreshSchedule().observeOutputBusy(
                    poll_now_us, dma_busy);
#endif
            return;
        }
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

    const uint32_t overlay_now_us = state.now_us;
    const informational_overlay::ProofOverrides proof =
        informational_overlay::proofOverrides();
    const bool holdover_active = informational_overlay::holdoverActive(false, proof);
    uint8_t holdover_level = informational_overlay::holdover_min_intensity;
    if (holdover_active) {
        if (!holdover_was_active_ ||
            static_cast<uint32_t>(overlay_now_us - last_holdover_redraw_us_) >=
                informational_overlay::holdover_redraw_interval_us) {
            if (!holdover_was_active_) {
                holdover_breather_.reset(overlay_now_us);
                last_holdover_redraw_us_ = overlay_now_us;
            } else {
                // Preserve the sampling grid; skip overdue samples rather
                // than drifting the cadence or replaying stale animation.
                const uint32_t elapsed = overlay_now_us - last_holdover_redraw_us_;
                last_holdover_redraw_us_ += elapsed /
                    informational_overlay::holdover_redraw_interval_us *
                    informational_overlay::holdover_redraw_interval_us;
            }
            holdover_level = holdover_breather_.update(overlay_now_us);
        } else if (overlay_frame_initialized_) {
            holdover_level = last_overlay_frame_.holdover_intensity;
        }
        holdover_was_active_ = true;
    } else {
        holdover_breather_.reset(overlay_now_us);
        holdover_was_active_ = false;
    }
    const informational_overlay::Frame overlay =
        informational_overlay::makeFrame(state, false, holdover_level, proof);
    if (!overlay_frame_initialized_ ||
        !informational_overlay::sameFrame(overlay, last_overlay_frame_)) {
        force_refresh = true;
        frame_dirty_ = true;
        last_overlay_frame_ = overlay;
        overlay_frame_initialized_ = true;
    }
    if (force_refresh) frame_dirty_ = true;

    updateFrame(state, force_refresh, overlay);
    informational_overlay::isolateLevelProof(pixels_, overlay);
    const bool effect_dither_requested =
        clock_transition_.effectDitherRequested();
    const bool effect_request_changed =
        effect_dither_requested != effect_dither_requested_;
    effect_dither_requested_ = effect_dither_requested;
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
    if (effect_request_changed && !dither_schedule.hasUserRateSelection()) {
        const uint16_t policy_rate = effect_dither_requested
            ? effect_temporal_dither_rate_hz
            : temporal_dither_default_rate_hz;
        dither_schedule.setRateHzForEffect(policy_rate, micros());
    }
    const TemporalDitherMode requested_dither_mode =
        resolveTemporalDitherMode(effect_dither_requested,
                                  temporalDitherMode());
#else
    const TemporalDitherMode requested_dither_mode =
        resolveProductionEffectDitherMode(effect_dither_requested);
#endif
    const bool dither_mode_changed = requested_dither_mode != active_dither_mode_;
    if (dither_mode_changed || effect_request_changed) {
        // Reset on both edges so a temporary effect cannot inherit or leave
        // residual carry in the user's ordinary output mode.
        temporal_dither_.reset();
        active_dither_mode_ = requested_dither_mode;
        if (active_dither_mode_ == TemporalDitherMode::off)
            dither_schedule.stop();
        else {
#if !defined(AAC_BUILD_PROFILE_DEVELOPMENT)
            // Production effects have one fixed, physically selected target;
            // development rate controls remain diagnostic-only.
            dither_schedule.setRateHzForEffect(
                effect_temporal_dither_rate_hz, micros());
#endif
            dither_schedule.reset(micros());
        }
        frame_dirty_ = true;
    }
    if (brightness_changed || color_changed) temporal_dither_.reset();
    const bool dither_enabled = active_dither_mode_ != TemporalDitherMode::off;
    const bool dither_refresh_due = dither_enabled &&
        dither_schedule.due(micros());
    if (dither_refresh_due) frame_dirty_ = true;

    if (frame_dirty_) {
        uint8_t output_brightness = brightness();
        const auto transfer = display_appearance::transferMode();
        bool clock_frame = true;
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
        clock_frame = testPattern() == TestPattern::clock;
        output_brightness = testPatternBrightness(testPattern(), brightness());
        if (clock_frame)
            output_brightness = proofBrightness(proofMode(), brightness());
#endif
        // Freeze final output words before DMA starts. Stable local red is
        // already in device-code units, so it is not crushed by brightness 2
        // or brightened by development comparison curves. Other pixels retain
        // their existing brightness/transfer path exactly.
        const bool prepared = dither_enabled
            ? wire_frame_.prepareDithered(pixels_, output_brightness, transfer,
                                          temporal_dither_,
                                          active_dither_mode_)
            : wire_frame_.prepare(pixels_, output_brightness, transfer);
        if (!prepared) return;
        if (overlay.level_proof >= 0 || (clock_frame && overlay.holdover))
            wire_frame_.paintOutputPixel(31, 3,
                informational_overlay::outputRed(overlay));
        if (!wire_frame_.start()) return;
        dma_channel_config dma_config = dma_channel_get_default_config(dma_channel_);
        channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
        channel_config_set_read_increment(&dma_config, true);
        channel_config_set_write_increment(&dma_config, false);
        channel_config_set_dreq(&dma_config, pio_get_dreq(pio0, pio_sm_, true));
        channel_config_set_high_priority(&dma_config, true);
        __dmb(); // Publish all prepared SRAM words before enabling the DMA reader.
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
        const uint32_t frame_start_us = micros();
#endif
        dma_channel_configure(dma_channel_, &dma_config, &pio0->txf[pio_sm_],
                              wire_frame_.words(), pixel_count, true);
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
        if (dither_enabled)
            dither_schedule.recordFrameStart(frame_start_us);
#endif
        frame_dirty_ = false;
    }
}
}
#endif
