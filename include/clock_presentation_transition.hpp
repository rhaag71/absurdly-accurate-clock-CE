#pragma once

#include <cstdint>

#include "clock_dissolve.hpp"
#include "clock_recenter_slide.hpp"
#include "compact_clock_font.hpp"
#include "presentation_time_event.hpp"

namespace clock_presentation_transition {

struct UpdateResult {
  UpdateResult(bool changed_value = false,
               presentation_time_event::Event event_value =
                   presentation_time_event::Event::none)
      : changed(changed_value), event(event_value) {}

  bool changed;
  presentation_time_event::Event event;
};

// Owns only time-glyph presentation history. Authoritative time, civil-time
// conversion, informational overlays, and WS2812 transport remain independent.
class State {
 public:
  UpdateResult update(bool valid, uint8_t civil_hour, uint8_t civil_minute,
                      const char* text, int centered_x, uint32_t now_us) {
    const presentation_time_event::Event event =
        event_resolver_.update(valid, civil_hour, civil_minute);

    if (!valid) {
      const bool changed = has_text_ || dissolve_.active() ||
                           recenter_.initialized();
      clearVisualState();
      return {changed, event};
    }

    const bool text_changed = !has_text_ || !sameText(current_text_, text);

    // A newer displayed time invalidates the old visual source material. The
    // latest valid presentation wins and settles directly; no queue is kept.
    if (dissolve_.active() && text_changed) {
      settleDirect(text, centered_x, now_us);
      return {true, event};
    }

    if (dissolve_.active()) {
      const clock_dissolve::Advance advance = dissolve_.update(now_us);
      if (advance == clock_dissolve::Advance::completed) {
        // The complete new glyphs are now visible at the old origin. Only now
        // may the verified layout recenter slide begin.
        recenter_.update(true, dissolve_.targetX(), now_us);
      }
      return {advance != clock_dissolve::Advance::none, event};
    }

    if (presentation_time_event::isMajorSpecial(event)) {
      // Special choreography is reserved but not implemented yet. Its layout
      // contract is honored by settling directly at the final centered origin.
      settleDirect(text, centered_x, now_us);
      return {true, event};
    }

    if (event == presentation_time_event::Event::ordinary_hour && has_text_) {
      const int visual_x = recenter_.currentX();
      if (dissolve_.start(current_text_, text, visual_x, centered_x, now_us)) {
        copyText(current_text_, text);
        return {true, event};
      }
      settleDirect(text, centered_x, now_us);
      return {true, event};
    }

    if (text_changed) {
      copyText(current_text_, text);
      has_text_ = true;
    }
    const bool position_changed = recenter_.update(true, centered_x, now_us);
    return {text_changed || position_changed, event};
  }

  void reset() {
    event_resolver_.reset();
    clearVisualState();
  }

  template <typename Pixels>
  bool render(Pixels& pixels, const ws2812_display::Rgb& background,
              const ws2812_display::Rgb& clock_color) const {
    if (!has_text_) {
      pixels.fill(background);
      return true;
    }
    if (dissolve_.active()) {
      dissolve_.render(pixels, background, clock_color);
      return true;
    }
    pixels.fill(background);
    return compact_clock_font::drawText(pixels, current_text_,
                                        recenter_.currentX(), 0, clock_color);
  }

  bool hasText() const { return has_text_; }
  const char* text() const { return current_text_; }
  bool dissolveActive() const { return dissolve_.active(); }
  bool recenterActive() const { return recenter_.active(); }
  int currentX() const { return recenter_.currentX(); }
  int targetX() const { return recenter_.targetX(); }
  const clock_dissolve::State& dissolve() const { return dissolve_; }

 private:
  static constexpr uint8_t kTextCapacity = 6U;

  static bool sameText(const char* left, const char* right) {
    for (uint8_t i = 0; i < kTextCapacity; ++i) {
      if (left[i] != right[i]) {
        return false;
      }
      if (left[i] == '\0') {
        return true;
      }
    }
    return true;
  }

  static void copyText(char* destination, const char* source) {
    uint8_t i = 0U;
    for (; i + 1U < kTextCapacity && source[i] != '\0'; ++i) {
      destination[i] = source[i];
    }
    destination[i] = '\0';
    for (++i; i < kTextCapacity; ++i) {
      destination[i] = '\0';
    }
  }

  void settleDirect(const char* text, int centered_x, uint32_t now_us) {
    dissolve_.reset();
    recenter_.reset();
    recenter_.update(true, centered_x, now_us);
    copyText(current_text_, text);
    has_text_ = true;
  }

  void clearVisualState() {
    dissolve_.reset();
    recenter_.reset();
    current_text_[0] = '\0';
    has_text_ = false;
  }

  presentation_time_event::Resolver event_resolver_{};
  clock_dissolve::State dissolve_{};
  clock_recenter_slide::State recenter_{};
  char current_text_[kTextCapacity]{};
  bool has_text_ = false;
};

}  // namespace clock_presentation_transition
