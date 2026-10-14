#pragma once

#include <cstddef>
#include <cstdint>

#include "ws2812_brightness.hpp"
#include "ws2812_pixels.hpp"

namespace ws2812_display {

constexpr uint32_t temporal_dither_period_us = 10000U;
constexpr uint32_t temporal_dither_target_hz =
    1000000U / temporal_dither_period_us;

enum class TemporalDitherMode : uint8_t { off, basic, perceptual };
constexpr uint8_t perceptual_deadband_remainder = 8U;

inline TemporalDitherMode resolveTemporalDitherMode(
    bool effect_requests_perceptual, TemporalDitherMode user_mode) {
  return effect_requests_perceptual ? TemporalDitherMode::perceptual : user_mode;
}

// Per-channel remainder carry restores fractional brightness across frames.
// The state is separate for every wire-order pixel and RGB channel.
class TemporalDither {
 public:
  TemporalDither() { reset(); }

  void reset() {
    for (std::size_t i = 0U; i < pixel_count * 3U; ++i) {
      remainder_[i] = 0U;
      input_[i] = 0U;
    }
    brightness_initialized_ = false;
  }

  Rgb scale(Rgb color, uint8_t brightness, std::size_t raw_index,
            TemporalDitherMode mode = TemporalDitherMode::basic) {
    if (raw_index >= pixel_count) return {0U, 0U, 0U};
    if (!brightness_initialized_ || brightness != previous_brightness_) {
      for (std::size_t i = 0U; i < pixel_count * 3U; ++i)
        remainder_[i] = mode == TemporalDitherMode::perceptual
            ? phaseFor(i) : 0U;
      previous_brightness_ = brightness;
      brightness_initialized_ = true;
    }
    const std::size_t offset = raw_index * 3U;
    return {scaleChannel(color.red, brightness, input_[offset],
                         remainder_[offset], offset, mode),
            scaleChannel(color.green, brightness, input_[offset + 1U],
                         remainder_[offset + 1U], offset + 1U, mode),
            scaleChannel(color.blue, brightness, input_[offset + 2U],
                         remainder_[offset + 2U], offset + 2U, mode)};
  }

 private:
  static uint8_t scaleChannel(uint8_t channel, uint8_t brightness,
                              uint8_t& previous_input,
                              uint8_t& remainder_accumulator,
                              std::size_t channel_index,
                              TemporalDitherMode mode) {
    if (channel != previous_input) {
      previous_input = channel;
      remainder_accumulator = mode == TemporalDitherMode::perceptual
          ? phaseFor(channel_index) : 0U;
    }
    if (channel == 0U || brightness == 0U || brightness == 255U) {
      remainder_accumulator = 0U;
      return ws2812_display::scaleChannel(channel, brightness);
    }

    const uint16_t product =
        static_cast<uint16_t>(channel) * static_cast<uint16_t>(brightness);
    const uint8_t whole = static_cast<uint8_t>(product / 255U);
    const uint8_t remainder = static_cast<uint8_t>(product % 255U);
    if (remainder == 0U) {
      remainder_accumulator = 0U;
      return whole;
    }

    // Avoid a rare extra device-code pulse when its fractional benefit is
    // below 8/255 of one output code (~3.1%). This intentionally favors
    // perceptual stability over exact averaging for tiny errors.
    if (mode == TemporalDitherMode::perceptual &&
        remainder <= perceptual_deadband_remainder) {
      remainder_accumulator = 0U;
      return whole;
    }

    uint16_t accumulated =
        static_cast<uint16_t>(remainder_accumulator) + remainder;
    if (accumulated >= 255U) {
      accumulated -= 255U;
      remainder_accumulator = static_cast<uint8_t>(accumulated);
      return static_cast<uint8_t>(whole + 1U);
    }
    remainder_accumulator = static_cast<uint8_t>(accumulated);
    return whole;
  }

  static uint8_t phaseFor(std::size_t channel_index) {
    // A compact integer hash gives each raw-pixel/channel a stable, distinct
    // starting phase without RNG state or additional per-channel storage.
    uint32_t value = static_cast<uint32_t>(channel_index) + 0x9e3779b9U;
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return static_cast<uint8_t>(value % 255U);
  }

  uint8_t input_[pixel_count * 3U] = {};
  uint8_t remainder_[pixel_count * 3U] = {};
  uint8_t previous_brightness_ = 0U;
  bool brightness_initialized_ = false;
};

static_assert(sizeof(TemporalDither) == pixel_count * 6U + 2U,
              "temporal dither stores input and remainder per channel");

class TemporalDitherRefreshSchedule {
 public:
  void reset(uint32_t now_us) {
    last_refresh_us_ = now_us;
    active_ = true;
  }

  void stop() { active_ = false; }

  bool due(uint32_t now_us) {
    if (!active_) return false;
    const uint32_t elapsed = now_us - last_refresh_us_;
    if (elapsed < temporal_dither_period_us) return false;
    // Skip missed intervals: a delayed service sends one current frame only.
    last_refresh_us_ +=
        elapsed / temporal_dither_period_us * temporal_dither_period_us;
    return true;
  }

 private:
  uint32_t last_refresh_us_ = 0U;
  bool active_ = false;
};

#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
inline TemporalDitherMode& temporalDitherModeStorage() {
  static TemporalDitherMode mode = TemporalDitherMode::off;
  return mode;
}
inline uint32_t& temporalDitherGenerationStorage() {
  static uint32_t generation = 0U;
  return generation;
}
inline TemporalDitherMode temporalDitherMode() {
  return temporalDitherModeStorage();
}
inline bool temporalDitherEnabled() {
  return temporalDitherMode() != TemporalDitherMode::off;
}
inline const char* temporalDitherModeName(TemporalDitherMode mode) {
  switch (mode) {
    case TemporalDitherMode::basic: return "basic";
    case TemporalDitherMode::perceptual: return "perceptual";
    default: return "off";
  }
}
inline void setTemporalDitherMode(TemporalDitherMode mode) {
  if (temporalDitherModeStorage() == mode) return;
  temporalDitherModeStorage() = mode;
  ++temporalDitherGenerationStorage();
}
inline void setTemporalDitherEnabled(bool enabled) {
  setTemporalDitherMode(enabled ? TemporalDitherMode::basic
                                : TemporalDitherMode::off);
}
inline uint32_t temporalDitherGeneration() {
  return temporalDitherGenerationStorage();
}

class TemporalDitherRefreshTracker {
 public:
  TemporalDitherRefreshTracker() : observed_(temporalDitherGeneration()) {}
  void reset() { observed_ = temporalDitherGeneration(); }
  bool consumeChange() {
    const uint32_t current = temporalDitherGeneration();
    if (current == observed_) return false;
    observed_ = current;
    return true;
  }

 private:
  uint32_t observed_;
};
#else
inline TemporalDitherMode temporalDitherMode() {
  return TemporalDitherMode::off;
}
inline const char* temporalDitherModeName(TemporalDitherMode) {
  return "off";
}
inline void setTemporalDitherMode(TemporalDitherMode) {}
inline void setTemporalDitherEnabled(bool) {}
inline bool temporalDitherEnabled() { return false; }
class TemporalDitherRefreshTracker {
 public:
  void reset() {}
  bool consumeChange() { return false; }
};
#endif

}  // namespace ws2812_display
