#pragma once

#include <cstdint>

#include "clock_dissolve.hpp"
#include "clock_noon_effect.hpp"
#include "clock_recenter_slide.hpp"
#include "clock_six_am_effect.hpp"
#include "clock_six_pm_effect.hpp"
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
                           recenter_.initialized() || six_pm_.primaryActive() ||
                           six_pm_.tintActive() || six_am_.primaryActive() ||
                           six_am_.tintActive() || noon_.active();
      clearVisualState();
      return {changed, event};
    }

    const bool text_changed = !has_text_ || !sameText(current_text_, text);

    if (noon_.active()) {
      if (text_changed || civil_hour != 12U || civil_minute != 0U) {
        settleDirect(text, centered_x, now_us);
        return {true, event};
      }
      const clock_noon_effect::AdvanceResult advance = noon_.update(now_us);
      return {advance.changed, event};
    }

    if (six_am_.primaryActive()) {
      if (text_changed || civil_hour != 6U || civil_minute != 0U) {
        settleDirect(text, centered_x, now_us);
        return {true, event};
      }
      const clock_six_am_effect::AdvanceResult advance =
          six_am_.update(now_us, civil_hour, civil_minute);
      return {advance.changed, event};
    }

    if (six_pm_.primaryActive()) {
      if (text_changed || civil_hour != 18U || civil_minute != 0U) {
        settleDirect(text, centered_x, now_us);
        return {true, event};
      }
      const clock_six_pm_effect::AdvanceResult advance =
          six_pm_.update(now_us, civil_hour, civil_minute);
      return {advance.changed, event};
    }

    const clock_six_pm_effect::AdvanceResult tint_update =
        six_pm_.updateTint(civil_hour, civil_minute);
    const clock_six_am_effect::AdvanceResult morning_tint_update =
        six_am_.updateTint(civil_hour, civil_minute);

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
      if (event == presentation_time_event::Event::six_pm) {
        dissolve_.reset();
        recenter_.reset();
        recenter_.update(true, centered_x, now_us);
        copyText(current_text_, text);
        has_text_ = true;
        if (six_pm_.start(text, centered_x, now_us)) {
          return {true, event};
        }
        six_pm_.reset();
        return {true, event};
      }
      if (event == presentation_time_event::Event::six_am) {
        six_pm_.reset();
        six_am_.reset();
        dissolve_.reset();
        recenter_.reset();
        recenter_.update(true, centered_x, now_us);
        copyText(current_text_, text);
        has_text_ = true;
        if (six_am_.start(text, centered_x, now_us)) return {true, event};
        settleDirect(text, centered_x, now_us);
        return {true, event};
      }
      if (event == presentation_time_event::Event::noon && has_text_) {
        const int visual_x = recenter_.currentX();
        six_am_.reset();
        six_pm_.reset();
        dissolve_.reset();
        recenter_.reset();
        recenter_.update(true, centered_x, now_us);
        if (noon_.start(current_text_, visual_x, text, centered_x, now_us)) {
          copyText(current_text_, text);
          has_text_ = true;
          return {true, event};
        }
        settleDirect(text, centered_x, now_us);
        return {true, event};
      }
      // Midnight choreography remains reserved; its layout contract is
      // honored by settling directly at the final centered origin.
      six_am_.reset();
      six_pm_.reset();
      noon_.reset();
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
    return {text_changed || position_changed || tint_update.changed ||
                morning_tint_update.changed,
            event};
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
    if (six_pm_.primaryActive()) {
      six_pm_.render(pixels, background, clock_color);
      return true;
    }
    if (six_am_.primaryActive()) {
      six_am_.render(pixels, background);
      return true;
    }
    if (noon_.active()) {
      noon_.render(pixels, background, clock_color);
      return true;
    }
    pixels.fill(background);
    return compact_clock_font::drawText(pixels, current_text_,
                                        recenter_.currentX(), 0,
                                        six_am_.tintActive()
                                            ? six_am_.tintedColor(clock_color)
                                            : six_pm_.tintedColor(clock_color));
  }

  bool hasText() const { return has_text_; }
  const char* text() const { return current_text_; }
  bool dissolveActive() const { return dissolve_.active(); }
  bool recenterActive() const { return recenter_.active(); }
  bool sixPmActive() const { return six_pm_.primaryActive(); }
  bool sixAmActive() const { return six_am_.primaryActive(); }
  bool noonActive() const { return noon_.active(); }
  bool noonDitherRequested() const { return noon_.ditherRequested(); }
  // Effect requests are presentation-owned and never mutate development
  // console selection. SixAM covers its animated sunrise/wash/dissolve only.
  bool sixAmDitherRequested() const {
    return six_am_.activeColorDitherRequested();
  }
  bool sixPmDitherRequested() const {
    return six_pm_.colorDitherRequested();
  }
  bool effectDitherRequested() const {
    return sixAmDitherRequested() || sixPmDitherRequested() ||
           noonDitherRequested();
  }
  bool eveningTintActive() const { return six_pm_.tintActive(); }
  bool morningTintActive() const { return six_am_.tintActive(); }
  uint8_t eveningAmberPercent() const { return six_pm_.amberPercent(); }
  uint8_t morningBluePercent() const { return six_am_.bluePercent(); }
  const clock_six_pm_effect::State& sixPmEffect() const { return six_pm_; }
  const clock_six_am_effect::State& sixAmEffect() const { return six_am_; }
  const clock_noon_effect::State& noonEffect() const { return noon_; }
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
    six_am_.reset();
    six_pm_.reset();
    noon_.reset();
    recenter_.reset();
    recenter_.update(true, centered_x, now_us);
    copyText(current_text_, text);
    has_text_ = true;
  }

  void clearVisualState() {
    dissolve_.reset();
    six_am_.reset();
    six_pm_.reset();
    noon_.reset();
    recenter_.reset();
    current_text_[0] = '\0';
    has_text_ = false;
  }

  presentation_time_event::Resolver event_resolver_{};
  clock_dissolve::State dissolve_{};
  clock_six_am_effect::State six_am_{};
  clock_six_pm_effect::State six_pm_{};
  clock_noon_effect::State noon_{};
  clock_recenter_slide::State recenter_{};
  char current_text_[kTextCapacity]{};
  bool has_text_ = false;
};

}  // namespace clock_presentation_transition
