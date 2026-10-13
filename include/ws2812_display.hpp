#pragma once

#include <cstdint>
#include "clock_presentation_transition.hpp"
#include "presentation_state.hpp"
#include "ws2812_pixels.hpp"
#include "ws2812_wire_frame.hpp"
#include "ws2812_clock_color.hpp"
#include "display_transfer.hpp"
#include "informational_overlay.hpp"
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
#include "ws2812_test_pattern.hpp"
#endif

namespace ws2812_display {
// Selected driver owns GP8, PIO0 SM0, one DMA channel, and both its logical
// pixel buffer and immutable prepared wire stream. No DMA interrupt is used.
class Driver {
public:
    Driver() = default;
    void begin(const presentation::State& state);
    void service(const presentation::State& state);

private:
    Pixels pixels_;
    WireFrame wire_frame_;
    int dma_channel_ = -1;
    uint8_t pio_sm_ = 0;
    uint8_t program_offset_ = 0;
    BrightnessRefreshTracker brightness_refresh_;
    ClockColorRefreshTracker color_refresh_;
    display_appearance::TransferRefreshTracker transfer_refresh_;
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
    ProofModeRefreshTracker proof_mode_refresh_;
    TestPatternRefreshTracker test_pattern_refresh_;
    bool last_frame_was_pattern_ = false;
#endif
    informational_overlay::BreathingAnimator holdover_breather_;
    clock_presentation_transition::State clock_transition_;
    informational_overlay::Frame last_overlay_frame_;
    uint32_t last_holdover_redraw_us_ = 0;
    bool overlay_frame_initialized_ = false;
    bool holdover_was_active_ = false;
    bool initialized_ = false;
    bool frame_dirty_ = true;

    void updateFrame(const presentation::State& state, bool force_refresh,
                     const informational_overlay::Frame& overlay);

};
}
