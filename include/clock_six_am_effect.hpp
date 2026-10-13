#pragma once

#include <cstdint>

#include "clock_dissolve.hpp"

namespace clock_six_am_effect {

constexpr uint32_t kDarkBeatUs = 200000U;
constexpr uint32_t kSunRiseDurationUs = 3500000U;
constexpr uint32_t kSunTopHoldUs = 500000U;
// A visible hold lets the large sun and glyph shadow read before the wash.
constexpr uint32_t kBlueWashStartUs = kSunRiseDurationUs + kSunTopHoldUs;
constexpr uint32_t kBlueWashDurationUs = 2500000U;
constexpr uint32_t kSunDissolveBucketIntervalUs = 75000U;
constexpr uint8_t kSunDissolveBucketCount = 8U;
constexpr uint32_t kSunDissolveDurationUs =
    kSunDissolveBucketIntervalUs * kSunDissolveBucketCount;
constexpr uint32_t kAnimationFrameIntervalUs = 50000U;
constexpr uint8_t kBluePercentPerMinute = 20U;
constexpr int8_t kSunRadius = 12;
constexpr uint8_t kSunDiameter = 2U * kSunRadius + 1U;
constexpr uint8_t kBlueWashColumnJitterRows = 1U;
constexpr int8_t kSunStartCenterY = 20;
constexpr int8_t kSunEndCenterY = 6;

constexpr ws2812_display::Rgb kSunYellow{255U, 64U, 0U};
constexpr ws2812_display::Rgb kMorningBlue{64U, 160U, 255U};

enum class Phase : uint8_t { idle, dark_beat, sunrise };

struct AdvanceResult {
  AdvanceResult(bool changed_value = false, bool completed_value = false)
      : changed(changed_value), completed(completed_value) {}
  bool changed;
  bool completed;
};

// SixAM owns only the special event's time-glyph scene and compact glyph
// occupancy. It never changes authoritative time or adds a color framebuffer.
class State {
 public:
  bool start(const char* text, int centered_x, uint32_t now_us) {
    reset();
    if (!clock_dissolve::buildMask(glyph_mask_, text, centered_x)) return false;
    const int width = compact_clock_font::textWidth(text);
    sun_center_x_ = static_cast<int8_t>(centered_x + (width - 1) / 2);
    if (sun_center_x_ < 0 || sun_center_x_ >= ws2812_display::logical_width)
      return false;
    phase_ = Phase::dark_beat;
    phase_started_us_ = now_us;
    elapsed_us_ = 0U;
    last_frame_index_ = UINT32_MAX;
    return true;
  }

  void reset() {
    glyph_mask_.clear();
    phase_ = Phase::idle;
    phase_started_us_ = 0U;
    elapsed_us_ = 0U;
    last_frame_index_ = UINT32_MAX;
    sun_center_x_ = 0;
    tint_active_ = false;
    tint_minute_ = 0U;
    last_tint_minute_ = 0U;
  }

  AdvanceResult update(uint32_t now_us, uint8_t civil_hour,
                       uint8_t civil_minute) {
    if (phase_ == Phase::idle) return updateTint(civil_hour, civil_minute);
    if (phase_ == Phase::dark_beat) {
      if (static_cast<uint32_t>(now_us - phase_started_us_) < kDarkBeatUs)
        return {};
      phase_ = Phase::sunrise;
      phase_started_us_ = now_us;
      elapsed_us_ = 0U;
      last_frame_index_ = UINT32_MAX;
      return {true, false};
    }

    elapsed_us_ = static_cast<uint32_t>(now_us - phase_started_us_);
    constexpr uint32_t kWashCompleteUs = kBlueWashStartUs + kBlueWashDurationUs;
    constexpr uint32_t kPrimaryDurationUs =
        kWashCompleteUs + kSunDissolveDurationUs;
    if (elapsed_us_ >= kPrimaryDurationUs) {
      phase_ = Phase::idle;
      tint_active_ = true;
      tint_minute_ = 0U;
      last_tint_minute_ = 0U;
      return {true, true};
    }

    const uint32_t frame_index = elapsed_us_ / kAnimationFrameIntervalUs;
    if (frame_index == last_frame_index_) return {};
    last_frame_index_ = frame_index;
    return {true, false};
  }

  AdvanceResult updateTint(uint8_t civil_hour, uint8_t civil_minute) {
    if (!tint_active_) return {};
    if (civil_hour != 6U || civil_minute > 5U) {
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
  void render(Pixels& pixels, const ws2812_display::Rgb& background) const {
    pixels.fill(background);
    if (phase_ != Phase::sunrise) return;

    const int8_t center_y = sunCenterY(elapsed_us_);
    const uint8_t dissolve_progress = sunDissolveProgress(elapsed_us_);

    for (uint8_t y = 0U; y < ws2812_display::logical_height; ++y) {
      for (uint8_t x = 0U; x < ws2812_display::logical_width; ++x) {
        if (!sunPixelVisible(x, y, center_y, dissolve_progress)) continue;
        pixels.setLogical(x, y, kSunYellow);
      }
    }

    const bool wash_started = elapsed_us_ >= kBlueWashStartUs;
    for (uint8_t y = 0U; y < ws2812_display::logical_height; ++y) {
      for (uint8_t x = 0U; x < ws2812_display::logical_width; ++x) {
        if (!glyph_mask_.contains(x, y)) continue;
        if (wash_started &&
            static_cast<int>(y) <= blueFrontAt(x, elapsed_us_)) {
          pixels.setLogical(x, y, kMorningBlue);
        } else if (sunPixelVisible(x, y, center_y, dissolve_progress)) {
          // Opaque black is the sunrise's intended glyph-shaped shadow.
          pixels.setLogical(x, y, {0U, 0U, 0U});
        }
      }
    }
  }

  bool primaryActive() const { return phase_ != Phase::idle; }
  bool tintActive() const { return tint_active_; }
  uint8_t tintMinute() const { return tint_active_ ? tint_minute_ : 0U; }
  uint8_t bluePercent() const {
    return tint_active_
               ? static_cast<uint8_t>(100U -
                                      tint_minute_ * kBluePercentPerMinute)
               : 0U;
  }
  ws2812_display::Rgb tintedColor(
      const ws2812_display::Rgb& normal_color) const {
    if (!tint_active_) return normal_color;
    const uint8_t blue_percent = bluePercent();
    const uint16_t normal_percent = static_cast<uint16_t>(100U - blue_percent);
    return {blend(normal_color.red, kMorningBlue.red, normal_percent,
                  blue_percent),
            blend(normal_color.green, kMorningBlue.green, normal_percent,
                  blue_percent),
            blend(normal_color.blue, kMorningBlue.blue, normal_percent,
                  blue_percent)};
  }

  Phase phase() const { return phase_; }
  int sunCenterX() const { return sun_center_x_; }
  int sunCenterYNow() const { return sunCenterY(elapsed_us_); }
  int blueFrontAt(uint8_t x) const { return blueFrontAt(x, elapsed_us_); }
  static bool sunCircleContains(int dx, int dy) {
    return dx * dx + dy * dy <= kSunRadius * kSunRadius;
  }
  bool glyphMaskContains(uint8_t x, uint8_t y) const {
    return glyph_mask_.contains(x, y);
  }
  uint32_t elapsedUs() const { return elapsed_us_; }
  uint32_t primaryDurationUs() const {
    return kBlueWashStartUs + kBlueWashDurationUs + kSunDissolveDurationUs;
  }
  const clock_dissolve::ClockMask& glyphMask() const { return glyph_mask_; }

 private:
  static uint8_t blend(uint8_t normal, uint8_t blue,
                       uint16_t normal_percent, uint8_t blue_percent) {
    const uint16_t mixed = static_cast<uint16_t>(normal) * normal_percent +
                           static_cast<uint16_t>(blue) * blue_percent;
    return static_cast<uint8_t>((mixed + 50U) / 100U);
  }

  static int8_t sunCenterY(uint32_t elapsed_us) {
    if (elapsed_us >= kSunRiseDurationUs) return kSunEndCenterY;
    const uint32_t rise =
        elapsed_us * static_cast<uint32_t>(kSunStartCenterY - kSunEndCenterY) /
        kSunRiseDurationUs;
    return static_cast<int8_t>(kSunStartCenterY - rise);
  }

  static uint8_t coordinateHash(uint8_t x, uint8_t y) {
    const uint16_t value = static_cast<uint16_t>(x * 37U + y * 19U + x * x * 11U);
    return static_cast<uint8_t>((value ^ (value >> 4U)) & 0xffU);
  }

  static uint8_t sunBucket(uint8_t x, uint8_t y) {
    return static_cast<uint8_t>(coordinateHash(x, y) % kSunDissolveBucketCount);
  }

  static uint8_t sunDissolveProgress(uint32_t elapsed_us) {
    constexpr uint32_t kWashCompleteUs = kBlueWashStartUs + kBlueWashDurationUs;
    if (elapsed_us < kWashCompleteUs) return 0U;
    const uint32_t progress =
        (elapsed_us - kWashCompleteUs) / kSunDissolveBucketIntervalUs;
    return progress >= kSunDissolveBucketCount
               ? kSunDissolveBucketCount
               : static_cast<uint8_t>(progress);
  }

  bool sunPixelVisible(uint8_t x, uint8_t y, int8_t center_y,
                       uint8_t dissolve_progress) const {
    const int dx = static_cast<int>(x) - sun_center_x_;
    const int dy = static_cast<int>(y) - center_y;
    if (!sunCircleContains(dx, dy) ||
        dissolve_progress >= kSunDissolveBucketCount) {
      return false;
    }
    return sunBucket(x, y) >= dissolve_progress;
  }

  int8_t blueFrontAt(uint8_t x, uint32_t elapsed_us) const {
    if (elapsed_us < kBlueWashStartUs) return -2;
    const uint32_t wash_elapsed = elapsed_us - kBlueWashStartUs;
    const int base = static_cast<int>(
        wash_elapsed * (ws2812_display::logical_height + 1U) /
        kBlueWashDurationUs);
    const int jitter = static_cast<int>(coordinateHash(x, 3U) %
                                        (2U * kBlueWashColumnJitterRows + 1U)) -
                       kBlueWashColumnJitterRows;
    const int front = base + jitter;
    if (front < -1) return -1;
    if (front > ws2812_display::logical_height) {
      return static_cast<int8_t>(ws2812_display::logical_height);
    }
    return static_cast<int8_t>(front);
  }

  clock_dissolve::ClockMask glyph_mask_{};
  Phase phase_ = Phase::idle;
  uint32_t phase_started_us_ = 0U;
  uint32_t elapsed_us_ = 0U;
  uint32_t last_frame_index_ = UINT32_MAX;
  int8_t sun_center_x_ = 0;
  uint8_t tint_minute_ = 0U;
  uint8_t last_tint_minute_ = 0U;
  bool tint_active_ = false;
};

}  // namespace clock_six_am_effect
