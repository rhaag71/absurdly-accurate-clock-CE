#pragma once

#include <cstdint>

#include "clock_dissolve.hpp"

namespace clock_noon_effect {

// These conservative first-pass timings keep the outward motion, clear beat,
// inward reassembly, and recognition hold distinct while remaining tunable.
constexpr uint32_t kExplosionUs = 750000U;
constexpr uint32_t kMidpointUs = 120000U;
constexpr uint32_t kReassemblyUs = 750000U;
constexpr uint32_t kSettleUs = 300000U;
constexpr uint16_t kExplosionReleaseRangeMs = 160U;
constexpr uint16_t kReassemblyReleaseRangeMs = 140U;
constexpr uint32_t kExplosionColorAttackUs = 35000U;
constexpr uint32_t kReassemblyColorReturnUs = 100000U;
constexpr uint8_t kMaximumParticles = 64U;
constexpr ws2812_display::Rgb kHotOrange{255U, 96U, 0U};
constexpr ws2812_display::Rgb kHotRed{255U, 0U, 0U};
constexpr ws2812_display::Rgb kHotAmber{255U, 160U, 0U};
constexpr ws2812_display::Rgb kHotPurple{255U, 0U, 255U};

enum class Phase : uint8_t { idle, explosion, midpoint, reassembly, settle };

struct AdvanceResult {
  AdvanceResult(bool changed_value = false, bool completed_value = false)
      : changed(changed_value), completed(completed_value) {}
  bool changed;
  bool completed;
};

// A particle starts at one occupied source pixel, travels to a deterministic
// point beyond the panel, then travels to one final-mask pixel. The compact
// variation byte independently staggers the two legs without an RNG or heap.
struct Particle {
  int8_t source_x;
  int8_t source_y;
  int8_t burst_x;
  int8_t burst_y;
  uint8_t final_x;
  uint8_t final_y;
  uint8_t variation;
};

static_assert(sizeof(Particle) == 7U,
              "Noon particle records must remain compact");

class State {
 public:
  bool start(const char* old_text, int old_x, const char* new_text,
             int final_x, uint32_t now_us) {
    reset();
    clock_dissolve::ClockMask old_mask;
    if (!clock_dissolve::buildMask(old_mask, old_text, old_x) ||
        !clock_dissolve::buildMask(final_mask_, new_text, final_x)) {
      reset();
      return false;
    }

    const uint8_t old_count = countPixels(old_mask);
    const uint8_t final_count = countPixels(final_mask_);
    particle_count_ = old_count > final_count ? old_count : final_count;
    if (old_count == 0U || final_count == 0U ||
        particle_count_ > kMaximumParticles) {
      reset();
      return false;
    }

    int left = ws2812_display::logical_width;
    int right = -1;
    int top = ws2812_display::logical_height;
    int bottom = -1;
    for (uint8_t y = 0U; y < ws2812_display::logical_height; ++y) {
      for (uint8_t x = 0U; x < ws2812_display::logical_width; ++x) {
        if (!old_mask.contains(x, y)) continue;
        if (x < left) left = x;
        if (x > right) right = x;
        if (y < top) top = y;
        if (y > bottom) bottom = y;
      }
    }
    const int center_x_twice = left + right;
    const int center_y_twice = top + bottom;
    for (uint8_t i = 0U; i < particle_count_; ++i) {
      const uint8_t source_index =
          static_cast<uint8_t>((static_cast<uint16_t>(i) * old_count) /
                               particle_count_);
      const uint8_t final_index =
          static_cast<uint8_t>((static_cast<uint16_t>(i) * final_count) /
                               particle_count_);
      uint8_t source_x = 0U;
      uint8_t source_y = 0U;
      uint8_t final_x_pixel = 0U;
      uint8_t final_y_pixel = 0U;
      if (!pixelAt(old_mask, source_index, source_x, source_y) ||
          !pixelAt(final_mask_, final_index, final_x_pixel, final_y_pixel)) {
        reset();
        return false;
      }

      const uint8_t variation = coordinateHash(source_x, source_y,
                                               final_x_pixel, final_y_pixel, i);
      const int relative_x = 2 * static_cast<int>(source_x) - center_x_twice;
      const int relative_y = 2 * static_cast<int>(source_y) - center_y_twice;
      int dx = relative_x + jitter(variation, 0U);
      int dy = relative_y + jitter(variation, 2U);
      // Preserve the radial sign for non-center pixels. Exact center-axis
      // pixels use hash variation so they still launch in useful directions.
      if (2 * static_cast<int>(source_x) < center_x_twice && dx >= 0) dx = -1;
      if (2 * static_cast<int>(source_x) > center_x_twice && dx <= 0) dx = 1;
      if (2 * static_cast<int>(source_y) < center_y_twice && dy >= 0) dy = -1;
      if (2 * static_cast<int>(source_y) > center_y_twice && dy <= 0) dy = 1;
      if (relative_x >= -4 && relative_x <= 4 && relative_y >= -4 &&
          relative_y <= 4) {
        // The glyph's central pixels get a reproducible mix of axial and
        // diagonal launches rather than collapsing into a left/right split.
        switch (i % 3U) {
          case 0U:
            dx = 0;
            dy = (variation & 0x04U) != 0U ? 1 : -1;
            break;
          case 1U:
            dx = (variation & 0x04U) != 0U ? 1 : -1;
            dy = 0;
            break;
          default:
            dx = (variation & 0x04U) != 0U ? 1 : -1;
            dy = (variation & 0x08U) != 0U ? 1 : -1;
            break;
        }
      }

      Particle& particle = particles_[i];
      particle.source_x = static_cast<int8_t>(source_x);
      particle.source_y = static_cast<int8_t>(source_y);
      burstPoint(source_x, source_y, dx, dy, variation,
                 particle.burst_x, particle.burst_y);
      particle.final_x = final_x_pixel;
      particle.final_y = final_y_pixel;
      particle.variation = variation;
    }

    phase_ = Phase::explosion;
    phase_started_us_ = now_us;
    last_render_time_us_ = now_us;
    return true;
  }

  void reset() {
    final_mask_.clear();
    particle_count_ = 0U;
    phase_ = Phase::idle;
    phase_started_us_ = 0U;
    last_render_time_us_ = 0U;
  }

  AdvanceResult update(uint32_t now_us) {
    if (phase_ == Phase::idle) return {};
    last_render_time_us_ = now_us;
    const uint32_t elapsed = static_cast<uint32_t>(now_us - phase_started_us_);
    switch (phase_) {
      case Phase::explosion:
        if (elapsed >= kExplosionUs) return enter(Phase::midpoint, now_us);
        return elapsed == 0U ? AdvanceResult{} : AdvanceResult(true, false);
      case Phase::midpoint:
        if (elapsed >= kMidpointUs) return enter(Phase::reassembly, now_us);
        return {};
      case Phase::reassembly:
        if (elapsed >= kReassemblyUs) return enter(Phase::settle, now_us);
        return elapsed == 0U ? AdvanceResult{} : AdvanceResult(true, false);
      case Phase::settle:
        if (elapsed >= kSettleUs) {
          phase_ = Phase::idle;
          phase_started_us_ = 0U;
          return {true, true};
        }
        return {};
      case Phase::idle:
        break;
    }
    return {};
  }

  template <typename Pixels>
  void render(Pixels& pixels, const ws2812_display::Rgb& background,
              const ws2812_display::Rgb& normal_color) const {
    pixels.fill(background);
    if (phase_ == Phase::idle || phase_ == Phase::midpoint) return;
    if (phase_ == Phase::settle) {
      paintFinal(pixels, normal_color);
      return;
    }

    const uint32_t elapsed =
        static_cast<uint32_t>(last_render_time_us_ - phase_started_us_);
    if (phase_ == Phase::explosion) {
      for (uint8_t i = 0U; i < particle_count_; ++i) {
        int x = 0;
        int y = 0;
        explosionPosition(i, elapsed, x, y);
        pixels.setLogical(x, y, explosionColor(i, elapsed, normal_color));
      }
      return;
    }
    for (uint8_t i = 0U; i < particle_count_; ++i) {
      int x = 0;
      int y = 0;
      reassemblyPosition(i, elapsed, x, y);
      pixels.setLogical(x, y, reassemblyColor(i, elapsed, normal_color));
    }
  }

  // Rendering is a pure projection of the latest service timestamp. Keeping
  // this setter separate lets display adapters update state before drawing.
  void setRenderTime(uint32_t now_us) { last_render_time_us_ = now_us; }

  Phase phase() const { return phase_; }
  bool active() const { return phase_ != Phase::idle; }
  // Dither only while particle colors are actively moving. The dark midpoint
  // and final normal-white settle use the ordinary steady output path.
  bool ditherRequested() const {
    return phase_ == Phase::explosion || phase_ == Phase::reassembly;
  }
  uint8_t particleCount() const { return particle_count_; }
  uint32_t phaseStartedUs() const { return phase_started_us_; }
  const Particle& particleAt(uint8_t index) const { return particles_[index]; }
  const clock_dissolve::ClockMask& finalMask() const { return final_mask_; }
  ws2812_display::Rgb hotColor(uint8_t index) const {
    // Hash-derived palette assignment gives a repeatable, roughly balanced
    // mix without grouping colors by the row-major source-pixel scan order.
    switch ((particles_[index].variation >> 6U) & 0x03U) {
      case 0U: return kHotRed;
      case 1U: return kHotOrange;
      case 2U: return kHotPurple;
      default: return kHotAmber;
    }
  }
  ws2812_display::Rgb explosionColor(
      uint8_t index, uint32_t elapsed_us,
      const ws2812_display::Rgb& normal_color) const {
    const uint32_t delay = explosionDelayUs(particles_[index]);
    if (elapsed_us <= delay) return normal_color;
    const uint32_t color_elapsed = elapsed_us - delay;
    const uint8_t progress = static_cast<uint8_t>(
        color_elapsed >= kExplosionColorAttackUs
            ? 255U
            : (color_elapsed * 255U) / kExplosionColorAttackUs);
    return blend(normal_color, hotColor(index), progress);
  }
  ws2812_display::Rgb reassemblyColor(
      uint8_t index, uint32_t elapsed_us,
      const ws2812_display::Rgb& normal_color) const {
    const uint32_t delay = reassemblyDelayUs(particles_[index]);
    const uint32_t duration = kReassemblyUs - delay;
    const uint32_t return_start = delay + duration - kReassemblyColorReturnUs;
    if (elapsed_us <= return_start) return hotColor(index);
    const uint32_t color_elapsed = elapsed_us - return_start;
    const uint8_t progress = static_cast<uint8_t>(
        color_elapsed >= kReassemblyColorReturnUs
            ? 255U
            : (color_elapsed * 255U) / kReassemblyColorReturnUs);
    return blend(hotColor(index), normal_color, progress);
  }

  void explosionPosition(uint8_t index, uint32_t elapsed_us,
                        int& x, int& y) const {
    const Particle& particle = particles_[index];
    const uint32_t delay = explosionDelayUs(particle);
    const uint32_t duration = kExplosionUs - delay;
    const uint8_t progress = progressForLeg(elapsed_us, delay, duration);
    x = interpolate(particle.source_x, particle.burst_x, progress);
    y = interpolate(particle.source_y, particle.burst_y, progress);
  }

  void reassemblyPosition(uint8_t index, uint32_t elapsed_us,
                          int& x, int& y) const {
    const Particle& particle = particles_[index];
    const uint32_t delay = reassemblyDelayUs(particle);
    const uint32_t duration = kReassemblyUs - delay;
    const uint8_t progress = progressForLeg(elapsed_us, delay, duration);
    x = interpolate(particle.burst_x, particle.final_x, progress);
    y = interpolate(particle.burst_y, particle.final_y, progress);
  }

 private:
  static uint8_t coordinateHash(uint8_t source_x, uint8_t source_y,
                                uint8_t final_x, uint8_t final_y,
                                uint8_t particle_index) {
    uint16_t value = static_cast<uint16_t>(source_x * 29U + source_y * 17U +
                                           final_x * 11U + final_y * 7U +
                                           particle_index * 43U);
    value ^= static_cast<uint16_t>(value >> 5U);
    value = static_cast<uint16_t>(value * 109U + 37U);
    return static_cast<uint8_t>(value ^ (value >> 8U));
  }

  static uint8_t countPixels(const clock_dissolve::ClockMask& mask) {
    uint8_t count = 0U;
    for (uint8_t y = 0U; y < ws2812_display::logical_height; ++y) {
      for (uint8_t x = 0U; x < ws2812_display::logical_width; ++x) {
        if (mask.contains(x, y)) ++count;
      }
    }
    return count;
  }

  static bool pixelAt(const clock_dissolve::ClockMask& mask, uint8_t index,
                      uint8_t& x_out, uint8_t& y_out) {
    uint8_t current = 0U;
    for (uint8_t y = 0U; y < ws2812_display::logical_height; ++y) {
      for (uint8_t x = 0U; x < ws2812_display::logical_width; ++x) {
        if (!mask.contains(x, y)) continue;
        if (current++ == index) {
          x_out = x;
          y_out = y;
          return true;
        }
      }
    }
    return false;
  }

  static int interpolate(int start, int end, uint8_t progress) {
    return start + ((end - start) * static_cast<int>(progress)) / 255;
  }

  static int jitter(uint8_t variation, uint8_t shift) {
    return static_cast<int>((variation >> shift) & 0x03U) - 1;
  }

  static void burstPoint(uint8_t x, uint8_t y, int dx, int dy,
                         uint8_t variation, int8_t& out_x, int8_t& out_y) {
    const int ax = dx < 0 ? -dx : dx;
    const int ay = dy < 0 ? -dy : dy;
    const int distance_x = dx < 0
                               ? static_cast<int>(x) + 1
                               : static_cast<int>(ws2812_display::logical_width - x);
    const int distance_y = dy < 0
                               ? static_cast<int>(y) + 1
                               : static_cast<int>(ws2812_display::logical_height - y);
    const bool exits_side = dy == 0 ||
                            (dx != 0 && distance_x * ay <= distance_y * ax);
    const int overshoot = 2 + static_cast<int>((variation >> 4U) % 4U);
    if (exits_side) {
      const int target_x = dx < 0
                               ? -1 - overshoot
                               : ws2812_display::logical_width + overshoot;
      const int travel_x = target_x - static_cast<int>(x);
      out_x = static_cast<int8_t>(target_x);
      out_y = static_cast<int8_t>(static_cast<int>(y) +
                                  (dy * travel_x) / dx);
    } else {
      const int target_y = dy < 0
                               ? -1 - overshoot
                               : ws2812_display::logical_height + overshoot;
      const int travel_y = target_y - static_cast<int>(y);
      out_y = static_cast<int8_t>(target_y);
      out_x = static_cast<int8_t>(static_cast<int>(x) +
                                  (dx * travel_y) / dy);
    }
  }

  static uint8_t progressForLeg(uint32_t elapsed, uint32_t delay,
                                uint32_t duration) {
    const uint32_t local = elapsed <= delay ? 0U : elapsed - delay;
    return static_cast<uint8_t>(
        local >= duration ? 255U : (local * 255U) / duration);
  }

  uint8_t explosionProgress(uint8_t index, uint32_t elapsed) const {
    return progressForLeg(elapsed, explosionDelayUs(particles_[index]),
                          kExplosionUs - explosionDelayUs(particles_[index]));
  }

  uint8_t reassemblyProgress(uint8_t index, uint32_t elapsed) const {
    return progressForLeg(elapsed, reassemblyDelayUs(particles_[index]),
                          kReassemblyUs - reassemblyDelayUs(particles_[index]));
  }

  static ws2812_display::Rgb blend(ws2812_display::Rgb from,
                                   ws2812_display::Rgb to,
                                   uint8_t amount) {
    const auto channel = [amount](uint8_t a, uint8_t b) {
      return static_cast<uint8_t>(
          (static_cast<uint16_t>(a) * (255U - amount) +
           static_cast<uint16_t>(b) * amount + 127U) /
          255U);
    };
    return {channel(from.red, to.red), channel(from.green, to.green),
            channel(from.blue, to.blue)};
  }

  static uint32_t explosionDelayUs(const Particle& particle) {
    return static_cast<uint32_t>(particle.variation %
                                 (kExplosionReleaseRangeMs + 1U)) * 1000U;
  }

  static uint32_t reassemblyDelayUs(const Particle& particle) {
    return static_cast<uint32_t>((particle.variation >> 2U) %
                                 (kReassemblyReleaseRangeMs + 1U)) * 1000U;
  }

  AdvanceResult enter(Phase phase, uint32_t now_us) {
    phase_ = phase;
    phase_started_us_ = now_us;
    return {true, false};
  }

  template <typename Pixels>
  void paintFinal(Pixels& pixels, const ws2812_display::Rgb& color) const {
    for (uint8_t y = 0U; y < ws2812_display::logical_height; ++y) {
      for (uint8_t x = 0U; x < ws2812_display::logical_width; ++x) {
        if (final_mask_.contains(x, y)) pixels.setLogical(x, y, color);
      }
    }
  }

  Particle particles_[kMaximumParticles]{};
  clock_dissolve::ClockMask final_mask_{};
  uint32_t phase_started_us_ = 0U;
  uint32_t last_render_time_us_ = 0U;
  uint8_t particle_count_ = 0U;
  Phase phase_ = Phase::idle;
};

}  // namespace clock_noon_effect
