#pragma once

#include <cstddef>
#include <cstdint>

#include "compact_clock_font.hpp"
#include "ws2812_pixels.hpp"

namespace clock_dissolve {

constexpr uint8_t kBucketCount = 8U;
constexpr uint32_t kBucketIntervalUs = 35000U;
constexpr uint32_t kMidpointHoldUs = 40000U;

// One bit per logical panel pixel. The dissolve retains glyph occupancy only,
// never a second RGB framebuffer.
class ClockMask {
 public:
  void clear() {
    for (uint8_t y = 0; y < ws2812_display::logical_height; ++y) {
      rows_[y] = 0U;
    }
  }

  void setLogical(int x, int y, bool on) {
    if (x < 0 || x >= ws2812_display::logical_width || y < 0 ||
        y >= ws2812_display::logical_height) {
      return;
    }
    const uint32_t bit = static_cast<uint32_t>(1UL << x);
    if (on) {
      rows_[y] |= bit;
    } else {
      rows_[y] &= ~bit;
    }
  }

  bool contains(uint8_t x, uint8_t y) const {
    return x < ws2812_display::logical_width &&
           y < ws2812_display::logical_height &&
           (rows_[y] & static_cast<uint32_t>(1UL << x)) != 0U;
  }

  bool operator==(const ClockMask& other) const {
    for (uint8_t y = 0; y < ws2812_display::logical_height; ++y) {
      if (rows_[y] != other.rows_[y]) {
        return false;
      }
    }
    return true;
  }

 private:
  uint32_t rows_[ws2812_display::logical_height]{};
};

static_assert(sizeof(ClockMask) == 32U, "clock mask must remain 256 bits");

inline bool buildMask(ClockMask& mask, const char* text, int x) {
  mask.clear();
  return compact_clock_font::drawText(mask, text, x, 0, true);
}

// A small coordinate hash assigns every logical position to a stable bucket.
// Mixing both axes avoids turning the dissolve into a row or column wipe.
inline uint8_t bucketFor(uint8_t x, uint8_t y) {
  uint16_t value = static_cast<uint16_t>(x * 29U + y * 17U + x * y * 7U);
  value ^= static_cast<uint16_t>(value >> 3U);
  return static_cast<uint8_t>(value & (kBucketCount - 1U));
}

enum class Phase : uint8_t {
  idle,
  old_out,
  midpoint,
  new_in,
};

enum class Advance : uint8_t {
  none,
  changed,
  completed,
};

class State {
 public:
  bool start(const char* old_text, const char* new_text, int visual_x,
             int target_x, uint32_t now_us) {
    reset();
    if (!buildMask(old_mask_, old_text, visual_x) ||
        !buildMask(new_mask_, new_text, visual_x)) {
      return false;
    }
    target_x_ = static_cast<int8_t>(target_x);
    phase_ = Phase::old_out;
    last_advance_us_ = now_us;
    return true;
  }

  void reset() {
    old_mask_.clear();
    new_mask_.clear();
    phase_ = Phase::idle;
    progress_ = 0U;
    target_x_ = 0;
    last_advance_us_ = 0U;
  }

  Advance update(uint32_t now_us) {
    if (phase_ == Phase::idle) {
      return Advance::none;
    }

    const uint32_t interval =
        phase_ == Phase::midpoint ? kMidpointHoldUs : kBucketIntervalUs;
    if (static_cast<uint32_t>(now_us - last_advance_us_) < interval) {
      return Advance::none;
    }

    // Advance at most once per service call. A delayed display service must
    // not produce a blocking catch-up burst.
    last_advance_us_ = now_us;
    if (phase_ == Phase::old_out) {
      ++progress_;
      if (progress_ == kBucketCount) {
        phase_ = Phase::midpoint;
        progress_ = 0U;
      }
      return Advance::changed;
    }

    if (phase_ == Phase::midpoint) {
      phase_ = Phase::new_in;
      progress_ = 0U;
      return Advance::changed;
    }

    ++progress_;
    if (progress_ == kBucketCount) {
      phase_ = Phase::idle;
      return Advance::completed;
    }
    return Advance::changed;
  }

  template <typename Pixels>
  void render(Pixels& pixels, const ws2812_display::Rgb& background,
              const ws2812_display::Rgb& clock_color) const {
    pixels.fill(background);
    if (phase_ == Phase::midpoint || phase_ == Phase::idle) {
      return;
    }

    const ClockMask& mask =
        phase_ == Phase::old_out ? old_mask_ : new_mask_;
    for (uint8_t y = 0; y < ws2812_display::logical_height; ++y) {
      for (uint8_t x = 0; x < ws2812_display::logical_width; ++x) {
        if (!mask.contains(x, y)) {
          continue;
        }
        const uint8_t bucket = bucketFor(x, y);
        const bool visible = phase_ == Phase::old_out
                                 ? bucket >= progress_
                                 : bucket < progress_;
        if (visible) {
          pixels.setLogical(x, y, clock_color);
        }
      }
    }
  }

  bool active() const { return phase_ != Phase::idle; }
  Phase phase() const { return phase_; }
  uint8_t progress() const { return progress_; }
  int targetX() const { return target_x_; }
  const ClockMask& oldMask() const { return old_mask_; }
  const ClockMask& newMask() const { return new_mask_; }

 private:
  ClockMask old_mask_{};
  ClockMask new_mask_{};
  Phase phase_ = Phase::idle;
  uint8_t progress_ = 0U;
  int8_t target_x_ = 0;
  uint32_t last_advance_us_ = 0U;
};

}  // namespace clock_dissolve
