#pragma once

#include <cstdint>

#include "clock_dissolve.hpp"

namespace clock_six_pm_effect {

constexpr uint32_t kClearBeatUs = 60000U;
// Drops move quickly; most of the rain's duration comes from deterministic,
// irregular release gaps. A small row preference favors lower destinations
// without imposing row barriers or preserving horizontal construction bands.
constexpr uint16_t kMinimumReleaseGapMs = 35U;
constexpr uint16_t kReleaseGapRangeMs = 51U;  // 35..85 ms, inclusive.
constexpr uint16_t kBottomBiasPerRow = 200U;
constexpr uint32_t kVerticalStepIntervalUs = 50000U;
constexpr uint8_t kMaximumFallingElements = 64U;
constexpr uint32_t kSettleUs = 550000U;
constexpr uint32_t kOutboundColumnIntervalUs = 40000U;
constexpr uint32_t kRightHoldUs = 350000U;
constexpr uint32_t kReturnColumnIntervalUs = 80000U;
constexpr uint8_t kEveningTintPercentPerMinute = 20U;

constexpr ws2812_display::Rgb kEveningAmber =
    display_appearance::reference_amber;

enum class Phase : uint8_t {
  idle,
  clear_beat,
  rain,
  settle,
  outbound,
  right_hold,
  returning,
};

struct AdvanceResult {
  AdvanceResult(bool changed_value = false, bool completed_value = false)
      : changed(changed_value), completed(completed_value) {}
  bool changed;
  bool completed;
};

// Six:00 has 51 occupied mask positions with the approved font. Keep room for
// the bounded mask representation and store each drop's own x, destination,
// release offset, and current row; current_y == final_y means landed.
struct FallingElement {
  uint16_t release_offset_ms;
  int8_t current_y;
  uint8_t final_x;
  uint8_t final_y;
};

static_assert(sizeof(FallingElement) <= 6U,
              "falling-pixel records must remain compact");

// One bounded SixPM choreography. Each final mask bit is a vertical faller;
// a global deterministic priority interleaves rows, with only a bottom-up
// preference. Variable gaps between releases provide the event's pacing.
class State {
 public:
  bool start(const char* new_text, int final_x, uint32_t now_us) {
    reset();
    if (!clock_dissolve::buildMask(final_mask_, new_text, final_x)) {
      return false;
    }

    left_x_ = ws2812_display::logical_width;
    right_x_ = -1;
    for (uint8_t y = 0; y < ws2812_display::logical_height; ++y) {
      for (uint8_t x = 0; x < ws2812_display::logical_width; ++x) {
        if (final_mask_.contains(x, y)) {
          if (x < left_x_) left_x_ = x;
          if (x > right_x_) right_x_ = x;
        }
      }
    }
    if (right_x_ < left_x_) {
      reset();
      return false;
    }

    element_count_ = 0U;
    for (uint8_t y = 0; y < ws2812_display::logical_height; ++y) {
      for (uint8_t x = 0; x < ws2812_display::logical_width; ++x) {
        if (!final_mask_.contains(x, y)) continue;
        if (element_count_ >= kMaximumFallingElements ||
            y > ws2812_display::logical_height - 2U) {
          reset();
          return false;
        }
        FallingElement& element = elements_[element_count_++];
        element.final_x = x;
        element.final_y = y;
        element.current_y = -1;
        element.release_offset_ms = UINT16_MAX;
      }
    }

    // Reuse each element's release field as an unassigned marker, so the
    // deterministic ordering needs no second array or additional state.
    uint32_t next_release_ms = 0U;
    for (uint8_t rank = 0U; rank < element_count_; ++rank) {
      uint8_t selected = element_count_;
      for (uint8_t i = 0U; i < element_count_; ++i) {
        if (elements_[i].release_offset_ms != UINT16_MAX) continue;
        if (selected == element_count_ ||
            comesBefore(elements_[i], elements_[selected])) {
          selected = i;
        }
      }
      if (selected == element_count_ || next_release_ms > UINT16_MAX) {
        reset();
        return false;
      }
      FallingElement& element = elements_[selected];
      element.release_offset_ms = static_cast<uint16_t>(next_release_ms);
      next_release_ms += releaseGapMs(element);
    }

    rain_duration_us_ = calculateRainDuration();
    phase_ = Phase::clear_beat;
    phase_started_us_ = now_us;
    last_frame_us_ = now_us;
    sweep_x_ = left_x_;
    tint_active_ = false;
    tint_minute_ = 0U;
    last_tint_minute_ = 0U;
    return true;
  }

  void reset() {
    final_mask_.clear();
    element_count_ = 0U;
    phase_ = Phase::idle;
    left_x_ = 0;
    right_x_ = -1;
    sweep_x_ = 0;
    rain_duration_us_ = 0U;
    phase_started_us_ = 0U;
    last_frame_us_ = 0U;
    tint_active_ = false;
    tint_minute_ = 0U;
    last_tint_minute_ = 0U;
  }

  AdvanceResult update(uint32_t now_us, uint8_t civil_hour,
                       uint8_t civil_minute) {
    if (phase_ == Phase::idle) {
      return updateTint(civil_hour, civil_minute);
    }

    const uint32_t elapsed = static_cast<uint32_t>(now_us - phase_started_us_);
    switch (phase_) {
      case Phase::clear_beat:
        if (elapsed >= kClearBeatUs) return enter(Phase::rain, now_us);
        return {};
      case Phase::rain:
        {
          const bool changed = advanceElements(elapsed);
          if (elapsed >= rain_duration_us_) {
            const AdvanceResult result = enter(Phase::settle, now_us);
            return {changed || result.changed, false};
          }
          if (changed) return {true, false};
        }
        return {};
      case Phase::settle:
        if (elapsed >= kSettleUs) {
          sweep_x_ = left_x_;
          return enter(Phase::outbound, now_us);
        }
        return {};
      case Phase::outbound:
        if (static_cast<uint32_t>(now_us - last_frame_us_) <
            kOutboundColumnIntervalUs) {
          return {};
        }
        last_frame_us_ = now_us;
        if (sweep_x_ < right_x_) {
          ++sweep_x_;
          if (sweep_x_ == right_x_) {
            phase_ = Phase::right_hold;
            phase_started_us_ = now_us;
            last_frame_us_ = now_us;
          }
          return {true, false};
        }
        return enter(Phase::right_hold, now_us);
      case Phase::right_hold:
        if (elapsed >= kRightHoldUs) return enter(Phase::returning, now_us);
        return {};
      case Phase::returning:
        if (static_cast<uint32_t>(now_us - last_frame_us_) <
            kReturnColumnIntervalUs) {
          return {};
        }
        last_frame_us_ = now_us;
        if (sweep_x_ > left_x_) {
          --sweep_x_;
          if (sweep_x_ == left_x_) {
            phase_ = Phase::idle;
            tint_active_ = true;
            tint_minute_ = 0U;
            last_tint_minute_ = 0U;
            return {true, true};
          }
          return {true, false};
        }
        phase_ = Phase::idle;
        tint_active_ = true;
        tint_minute_ = 0U;
        last_tint_minute_ = 0U;
        return {true, true};
      case Phase::idle:
        break;
    }
    return {};
  }

  // True when a minute step changed or invalidated the tint. Repeated service
  // calls within the same civil minute leave the blend stable.
  AdvanceResult updateTint(uint8_t civil_hour, uint8_t civil_minute) {
    if (!tint_active_) return {};
    if (civil_hour != 18U || civil_minute > 5U) {
      tint_active_ = false;
      tint_minute_ = 0U;
      return {true, false};
    }
    if (civil_minute == last_tint_minute_) return {};
    if (civil_minute != static_cast<uint8_t>(last_tint_minute_ + 1U)) {
      tint_active_ = false;
      tint_minute_ = 0U;
      return {true, false};
    }
    last_tint_minute_ = civil_minute;
    tint_minute_ = civil_minute;
    if (civil_minute == 5U) tint_active_ = false;
    return {true, false};
  }

  template <typename Pixels>
  void render(Pixels& pixels, const ws2812_display::Rgb& background,
              const ws2812_display::Rgb& normal_color) const {
    pixels.fill(background);
    if (phase_ == Phase::idle || phase_ == Phase::clear_beat ||
        phase_ == Phase::settle) {
      if (phase_ == Phase::settle) paintAll(pixels, normal_color);
      return;
    }
    if (phase_ == Phase::rain) {
      renderRain(pixels, normal_color);
      return;
    }
    if (phase_ == Phase::outbound || phase_ == Phase::right_hold) {
      paintAll(pixels, normal_color);
      paintColumn(pixels, sweep_x_, kEveningAmber);
      return;
    }
    if (phase_ == Phase::returning) {
      for (uint8_t y = 0; y < ws2812_display::logical_height; ++y) {
        for (uint8_t x = 0; x < ws2812_display::logical_width; ++x) {
          if (!final_mask_.contains(x, y)) continue;
          const ws2812_display::Rgb color =
              x >= sweep_x_ ? kEveningAmber : normal_color;
          pixels.setLogical(x, y, color);
        }
      }
    }
  }

  bool primaryActive() const { return phase_ != Phase::idle; }
  bool tintActive() const { return tint_active_; }
  uint8_t tintMinute() const { return tint_active_ ? tint_minute_ : 0U; }
  uint8_t amberPercent() const {
    return tint_active_
               ? static_cast<uint8_t>(100U -
                                      tint_minute_ * kEveningTintPercentPerMinute)
               : 0U;
  }
  Phase phase() const { return phase_; }
  int leftX() const { return left_x_; }
  int rightX() const { return right_x_; }
  int sweepX() const { return sweep_x_; }
  uint32_t rainDurationUs() const { return rain_duration_us_; }
  uint8_t elementCount() const { return element_count_; }
  const FallingElement& elementAt(uint8_t index) const {
    return elements_[index];
  }
  uint8_t releaseRankAt(uint8_t x, uint8_t y) const {
    const FallingElement* element = findElement(x, y);
    if (!element) return UINT8_MAX;
    uint8_t rank = 0U;
    for (uint8_t i = 0U; i < element_count_; ++i)
      if (elements_[i].release_offset_ms < element->release_offset_ms) ++rank;
    return rank;
  }
  uint32_t releaseOffsetAt(uint8_t x, uint8_t y) const {
    const FallingElement* element = findElement(x, y);
    return element ? static_cast<uint32_t>(element->release_offset_ms) * 1000U
                   : 0U;
  }
  uint32_t releaseGapAfterRankUs(uint8_t rank) const {
    for (uint8_t i = 0U; i < element_count_; ++i) {
      if (releaseRankAt(elements_[i].final_x, elements_[i].final_y) != rank)
        continue;
      return static_cast<uint32_t>(releaseGapMs(elements_[i])) * 1000U;
    }
    return 0U;
  }
  uint32_t landingOffsetAt(uint8_t x, uint8_t y) const {
    return releaseOffsetAt(x, y) +
           static_cast<uint32_t>(y + 1U) * kVerticalStepIntervalUs;
  }
  int fallingYAt(uint8_t x, uint8_t target_y, uint32_t elapsed_us) const {
    if (!final_mask_.contains(x, target_y)) return -2;
    const uint32_t release_us = releaseOffsetAt(x, target_y);
    const uint32_t elapsed_ms = elapsed_us / 1000U;
    if (elapsed_ms * 1000U < release_us) return -1;
    const int y = -1 + static_cast<int>(
                            (elapsed_ms * 1000U - release_us) /
                            kVerticalStepIntervalUs);
    return y < target_y ? y : target_y;
  }
  const clock_dissolve::ClockMask& finalMask() const { return final_mask_; }

  static ws2812_display::Rgb blendColor(
      const ws2812_display::Rgb& normal_color,
      const ws2812_display::Rgb& amber_color, uint8_t amber_percent) {
    const uint16_t normal_percent = static_cast<uint16_t>(100U - amber_percent);
    return {
        blendChannel(normal_color.red, amber_color.red, normal_percent,
                     amber_percent),
        blendChannel(normal_color.green, amber_color.green, normal_percent,
                     amber_percent),
        blendChannel(normal_color.blue, amber_color.blue, normal_percent,
                     amber_percent),
    };
  }

  ws2812_display::Rgb tintedColor(
      const ws2812_display::Rgb& normal_color) const {
    return blendColor(normal_color, kEveningAmber, amberPercent());
  }

 private:
  static uint8_t releaseHash(uint8_t x, uint8_t y) {
    const uint16_t value = static_cast<uint16_t>(x * 37U + y * 19U + x * x * 11U);
    return static_cast<uint8_t>((value ^ (value >> 4U)) & 0xffU);
  }

  static uint16_t releasePriority(const FallingElement& element) {
    const uint16_t hash_component =
        static_cast<uint16_t>(releaseHash(element.final_x, element.final_y)) * 8U;
    const uint16_t row_bias = static_cast<uint16_t>(
        (ws2812_display::logical_height - 2U - element.final_y) *
        kBottomBiasPerRow);
    return static_cast<uint16_t>(hash_component + row_bias);
  }

  static bool comesBefore(const FallingElement& a, const FallingElement& b) {
    const uint16_t a_priority = releasePriority(a);
    const uint16_t b_priority = releasePriority(b);
    if (a_priority != b_priority) return a_priority < b_priority;
    if (a.final_y != b.final_y) return a.final_y > b.final_y;
    return a.final_x < b.final_x;
  }

  static uint16_t releaseGapMs(const FallingElement& element) {
    return static_cast<uint16_t>(
        kMinimumReleaseGapMs +
        releaseHash(element.final_x, element.final_y) % kReleaseGapRangeMs);
  }

  const FallingElement* findElement(uint8_t x, uint8_t y) const {
    for (uint8_t i = 0U; i < element_count_; ++i) {
      if (elements_[i].final_x == x && elements_[i].final_y == y)
        return &elements_[i];
    }
    return nullptr;
  }

  uint32_t calculateRainDuration() const {
    uint32_t longest = 0U;
    for (uint8_t i = 0; i < element_count_; ++i) {
      const FallingElement& element = elements_[i];
      const uint32_t landing =
          static_cast<uint32_t>(element.release_offset_ms) * 1000U +
          static_cast<uint32_t>(element.final_y + 1U) * kVerticalStepIntervalUs;
      if (landing > longest) longest = landing;
    }
    return longest;
  }

  bool advanceElements(uint32_t elapsed_us) {
    bool visible_changed = false;
    const uint32_t elapsed_ms = elapsed_us / 1000U;
    for (uint8_t i = 0; i < element_count_; ++i) {
      FallingElement& element = elements_[i];
      if (element.current_y >= static_cast<int8_t>(element.final_y) ||
          elapsed_ms < element.release_offset_ms) {
        continue;
      }
      const uint32_t steps =
          (elapsed_ms - element.release_offset_ms) * 1000U /
          kVerticalStepIntervalUs;
      int next_y = -1 + static_cast<int>(steps);
      if (next_y > element.final_y) next_y = element.final_y;
      if (next_y > element.current_y) {
        element.current_y = static_cast<int8_t>(next_y);
        if (element.current_y >= 0) visible_changed = true;
      }
    }
    return visible_changed;
  }

  static uint8_t blendChannel(uint8_t normal, uint8_t amber,
                              uint16_t normal_percent,
                              uint8_t amber_percent) {
    const uint16_t mixed = static_cast<uint16_t>(normal) * normal_percent +
                           static_cast<uint16_t>(amber) * amber_percent;
    return static_cast<uint8_t>((mixed + 50U) / 100U);
  }

  AdvanceResult enter(Phase next, uint32_t now_us) {
    phase_ = next;
    phase_started_us_ = now_us;
    last_frame_us_ = now_us;
    return {true, false};
  }

  template <typename Pixels>
  void paintAll(Pixels& pixels, const ws2812_display::Rgb& color) const {
    for (uint8_t y = 0; y < ws2812_display::logical_height; ++y) {
      for (uint8_t x = 0; x < ws2812_display::logical_width; ++x) {
        if (final_mask_.contains(x, y)) pixels.setLogical(x, y, color);
      }
    }
  }

  template <typename Pixels>
  void paintColumn(Pixels& pixels, int x,
                   const ws2812_display::Rgb& color) const {
    if (x < 0 || x >= ws2812_display::logical_width) return;
    for (uint8_t y = 0; y < ws2812_display::logical_height; ++y) {
      if (final_mask_.contains(static_cast<uint8_t>(x), y)) {
        pixels.setLogical(x, y, color);
      }
    }
  }

  template <typename Pixels>
  void renderRain(Pixels& pixels,
                  const ws2812_display::Rgb& normal_color) const {
    for (uint8_t i = 0; i < element_count_; ++i) {
      const FallingElement& element = elements_[i];
      if (element.current_y >= 0) {
        pixels.setLogical(element.final_x, element.current_y, normal_color);
      }
    }
  }

  clock_dissolve::ClockMask final_mask_{};
  FallingElement elements_[kMaximumFallingElements]{};
  Phase phase_ = Phase::idle;
  int8_t left_x_ = 0;
  int8_t right_x_ = -1;
  int8_t sweep_x_ = 0;
  uint32_t rain_duration_us_ = 0U;
  uint32_t phase_started_us_ = 0U;
  uint32_t last_frame_us_ = 0U;
  uint8_t element_count_ = 0U;
  uint8_t tint_minute_ = 0U;
  uint8_t last_tint_minute_ = 0U;
  bool tint_active_ = false;
};

}  // namespace clock_six_pm_effect
