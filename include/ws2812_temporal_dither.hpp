#pragma once

#include <cstddef>
#include <cstdint>

#include "ws2812_brightness.hpp"
#include "ws2812_pixels.hpp"

namespace ws2812_display {

// Development's manual A/B mode keeps its prior 100 Hz default. Effect-owned
// rendering uses the separately selected fixed production target below.
constexpr uint16_t temporal_dither_default_rate_hz = 100U;
constexpr uint16_t effect_temporal_dither_rate_hz = 110U;
constexpr uint16_t temporal_dither_min_rate_hz = 50U;
// 110 Hz leaves only about 140 us beyond the calculated 8.95 ms complete
// wire/latch lifecycle. It is intentionally limited to short active effects.
constexpr uint16_t temporal_dither_max_rate_hz = 110U;
constexpr uint32_t temporal_dither_period_us =
    1000000U / temporal_dither_default_rate_hz;
constexpr uint32_t temporal_dither_target_hz =
    temporal_dither_default_rate_hz;
constexpr uint8_t temporal_dither_statistics_window = 100U;

enum class TemporalDitherMode : uint8_t { off, basic, perceptual };
constexpr uint8_t perceptual_deadband_remainder = 8U;

inline TemporalDitherMode resolveTemporalDitherMode(
    bool effect_requests_perceptual, TemporalDitherMode user_mode) {
  return effect_requests_perceptual ? TemporalDitherMode::perceptual : user_mode;
}

constexpr TemporalDitherMode resolveProductionEffectDitherMode(
    bool effect_requests_perceptual) {
  return effect_requests_perceptual ? TemporalDitherMode::perceptual
                                    : TemporalDitherMode::off;
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
    pending_ = false;
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
    has_last_frame_start_ = false;
#endif
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
    resetStatistics();
#endif
  }

  void stop() { active_ = false; }

  bool setRateHz(uint16_t rate_hz, uint32_t now_us) {
    if (!setRateHzInternal(rate_hz, now_us)) return false;
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
    user_rate_selected_ = true;
#endif
    return true;
  }

  // Effect policy may temporarily select 110 Hz without changing the
  // development console's independent A/B rate preference.
  bool setRateHzForEffect(uint16_t rate_hz, uint32_t now_us) {
    return setRateHzInternal(rate_hz, now_us);
  }

  bool hasUserRateSelection() const {
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
    return user_rate_selected_;
#else
    return false;
#endif
  }

 private:
  bool setRateHzInternal(uint16_t rate_hz, uint32_t now_us) {
    if (!validRateHz(rate_hz)) return false;
    rate_hz_ = rate_hz;
    period_us_ = 1000000U / rate_hz;
    if (active_) {
      last_refresh_us_ = now_us;
      pending_ = false;
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
      has_last_frame_start_ = false;
#endif
    }
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
    resetStatistics();
#endif
    return true;
  }

 public:

  static bool validRateHz(uint16_t rate_hz) {
    return rate_hz >= temporal_dither_min_rate_hz &&
           rate_hz <= temporal_dither_max_rate_hz;
  }

  uint16_t rateHz() const { return rate_hz_; }
  uint32_t periodUs() const { return period_us_; }

  bool due(uint32_t now_us) {
    if (!active_) return false;
    if (pending_) {
      const uint32_t additional = (now_us - last_refresh_us_) / period_us_;
      if (additional != 0U) {
        // The previous pending frame is obsolete; keep only the newest due
        // frame and account for skipped opportunities without catch-up.
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
        addSaturated(missed_intervals_, additional);
#endif
        last_refresh_us_ += additional * period_us_;
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
        eligible_at_us_ = last_refresh_us_;
#endif
      }
      return true;
    }
    const uint32_t elapsed = now_us - last_refresh_us_;
    if (elapsed < period_us_) return false;
    const uint32_t intervals = elapsed / period_us_;
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
    if (intervals > 1U) addSaturated(missed_intervals_, intervals - 1U);
#endif
    // Stay on the original interval grid and send one current frame only.
    last_refresh_us_ += intervals * period_us_;
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
    eligible_at_us_ = last_refresh_us_;
#endif
    pending_ = true;
    return true;
  }

#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
  // Called while a frame still owns the DMA/PIO stream. Each elapsed slot is
  // counted once; the newest remains pending for launch after release.
  void observeOutputBusy(uint32_t now_us, bool dma_busy) {
    if (!active_) return;
    const uint32_t intervals = (now_us - last_refresh_us_) / period_us_;
    if (intervals == 0U) return;
    addSaturated(output_busy_conflicts_, intervals);
    if (dma_busy) addSaturated(dma_busy_conflicts_, intervals);
    addSaturated(missed_intervals_, pending_ ? intervals : intervals - 1U);
    last_refresh_us_ += intervals * period_us_;
    eligible_at_us_ = last_refresh_us_;
    pending_ = true;
  }

  void recordFrameStart(uint32_t start_us) {
    if (has_last_frame_start_) recordInterval(start_us - last_frame_start_us_);
    last_frame_start_us_ = start_us;
    has_last_frame_start_ = true;
    if (pending_) {
      recordLatency(start_us - eligible_at_us_);
      pending_ = false;
    }
    addSaturated(frames_started_, 1U);
  }

  struct Statistics {
    uint16_t rate_hz = temporal_dither_default_rate_hz;
    uint32_t interval_us = 1000000U / temporal_dither_default_rate_hz;
    uint8_t interval_samples = 0U;
    uint32_t interval_min_us = 0U;
    uint32_t interval_average_us = 0U;
    uint32_t interval_max_us = 0U;
    uint8_t latency_samples = 0U;
    uint32_t latency_average_us = 0U;
    uint32_t latency_max_us = 0U;
    uint32_t frames_started = 0U;
    uint32_t missed_intervals = 0U;
    uint32_t dma_busy_conflicts = 0U;
    uint32_t output_busy_conflicts = 0U;
  };

  // Reporting consumes a bounded measurement window so each console query
  // describes the frames since the previous query rather than stale history.
  Statistics takeStatistics() {
    Statistics result;
    result.rate_hz = rate_hz_;
    result.interval_us = period_us_;
    result.interval_samples = interval_samples_;
    result.interval_min_us = interval_samples_ == 0U ? 0U : interval_min_us_;
    result.interval_average_us = interval_average_us_;
    result.interval_max_us = interval_max_us_;
    result.latency_samples = latency_samples_;
    result.latency_average_us = latency_average_us_;
    result.latency_max_us = latency_max_us_;
    result.frames_started = frames_started_;
    result.missed_intervals = missed_intervals_;
    result.dma_busy_conflicts = dma_busy_conflicts_;
    result.output_busy_conflicts = output_busy_conflicts_;
    resetStatistics();
    return result;
  }
#endif

 private:
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
  static void addSaturated(uint32_t& value, uint32_t add) {
    value = UINT32_MAX - value < add ? UINT32_MAX : value + add;
  }

  void recordInterval(uint32_t value) {
    if (interval_samples_ >= temporal_dither_statistics_window) return;
    if (interval_samples_ == 0U || value < interval_min_us_)
      interval_min_us_ = value;
    if (interval_samples_ == 0U || value > interval_max_us_)
      interval_max_us_ = value;
    ++interval_samples_;
    const int32_t error = static_cast<int32_t>(value) -
                          static_cast<int32_t>(interval_average_us_);
    interval_average_us_ = static_cast<uint32_t>(
        static_cast<int32_t>(interval_average_us_) + error / interval_samples_);
  }

  void recordLatency(uint32_t value) {
    if (latency_samples_ >= temporal_dither_statistics_window) return;
    if (latency_samples_ == 0U || value > latency_max_us_)
      latency_max_us_ = value;
    ++latency_samples_;
    const int32_t error = static_cast<int32_t>(value) -
                          static_cast<int32_t>(latency_average_us_);
    latency_average_us_ = static_cast<uint32_t>(
        static_cast<int32_t>(latency_average_us_) + error / latency_samples_);
  }

  void resetStatistics() {
    interval_samples_ = 0U;
    interval_min_us_ = 0U;
    interval_average_us_ = 0U;
    interval_max_us_ = 0U;
    latency_samples_ = 0U;
    latency_average_us_ = 0U;
    latency_max_us_ = 0U;
    frames_started_ = 0U;
    missed_intervals_ = 0U;
    dma_busy_conflicts_ = 0U;
    output_busy_conflicts_ = 0U;
  }
#endif

  uint32_t last_refresh_us_ = 0U;
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
  uint32_t eligible_at_us_ = 0U;
  uint32_t last_frame_start_us_ = 0U;
  uint32_t interval_min_us_ = 0U;
  uint32_t interval_average_us_ = 0U;
  uint32_t interval_max_us_ = 0U;
  uint32_t latency_average_us_ = 0U;
  uint32_t latency_max_us_ = 0U;
  uint32_t frames_started_ = 0U;
  uint32_t missed_intervals_ = 0U;
  uint32_t dma_busy_conflicts_ = 0U;
  uint32_t output_busy_conflicts_ = 0U;
#endif
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
  uint32_t period_us_ = temporal_dither_period_us;
  uint16_t rate_hz_ = temporal_dither_default_rate_hz;
#else
  uint32_t period_us_ = 1000000U / effect_temporal_dither_rate_hz;
  uint16_t rate_hz_ = effect_temporal_dither_rate_hz;
#endif
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
  uint8_t interval_samples_ = 0U;
  uint8_t latency_samples_ = 0U;
#endif
  bool active_ = false;
  bool pending_ = false;
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
  bool has_last_frame_start_ = false;
  bool user_rate_selected_ = false;
#endif
};

#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
inline TemporalDitherRefreshSchedule& temporalDitherRefreshSchedule() {
  static TemporalDitherRefreshSchedule schedule;
  return schedule;
}
#endif

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
