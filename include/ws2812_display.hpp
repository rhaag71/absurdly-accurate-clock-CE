#pragma once

#include <cstdint>
#include "presentation_state.hpp"
#include "ws2812_pixels.hpp"
#include "ws2812_clock_color.hpp"
#include "display_transfer.hpp"
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
#include "ws2812_test_pattern.hpp"
#endif

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
    display_appearance::TransferMode tx_transfer_ =
        display_appearance::TransferMode::off;
    BrightnessRefreshTracker brightness_refresh_;
    ClockColorRefreshTracker color_refresh_;
    display_appearance::TransferRefreshTracker transfer_refresh_;
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
    ProofModeRefreshTracker proof_mode_refresh_;
    TestPatternRefreshTracker test_pattern_refresh_;
    bool last_frame_was_pattern_ = false;
#endif
    char last_text_[6] = {};
    bool initialized_ = false;
    bool tx_active_ = false;
    bool frame_dirty_ = true;
    bool latch_pending_ = false;
    bool has_rendered_text_ = false;

    void updateFrame(const presentation::State& state, bool force_refresh);
    void sendChunk(uint32_t now_us);
};
}
