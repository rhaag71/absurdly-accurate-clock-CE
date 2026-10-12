#pragma once

#include <cstdint>
#include "ws2812_intensity.hpp"
#include "display_appearance.hpp"
#include "display_compositor.hpp"
#include "presentation_state.hpp"
#include "ws2812_pixels.hpp"

// Build-specific informational marks for Dad's 8x32 WS2812 panel. This module
// only translates presentation inputs into pixels; it never changes clock
// qualification or authoritative time.
namespace informational_overlay {
enum class Health : uint8_t { off, green, amber, red };
// Values are part of the development-console command contract.
enum class HealthProof : uint8_t { live, good, minimum, bad, off };
enum class HoldoverProof : uint8_t { live, on };

struct ProofOverrides {
    HealthProof health = HealthProof::live;
    HoldoverProof holdover = HoldoverProof::live;
    int8_t level = -1; // -1 follows overlays; 0..32 isolates a stable red pixel.
    uint8_t day = 0; // Zero follows the local civil date.
};

inline ProofOverrides& proofStorage() {
    static ProofOverrides value;
    return value;
}

inline ProofOverrides proofOverrides() {
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT) && defined(AAC_DISPLAY_WS2812)
    return proofStorage();
#else
    return {};
#endif
}

inline void setHealthProof(HealthProof value) {
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT) && defined(AAC_DISPLAY_WS2812)
    proofStorage().health = value;
#else
    (void)value;
#endif
}

inline void setHoldoverProof(HoldoverProof value) {
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT) && defined(AAC_DISPLAY_WS2812)
    proofStorage().holdover = value;
    proofStorage().level = -1;
#else
    (void)value;
#endif
}

inline void setLevelProof(int value) {
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT) && defined(AAC_DISPLAY_WS2812)
    proofStorage().level = value >= 0 && value <= 32 ? value : -1;
#else
    (void)value;
#endif
}

inline void setDayProof(uint8_t value) {
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT) && defined(AAC_DISPLAY_WS2812)
    proofStorage().day = value <= 31 ? value : 0;
#else
    (void)value;
#endif
}

inline Health liveHealth(const presentation::State& state) {
    const auto& clock = state.clock;
    if (!clock.gps_valid || !clock.pps_present || !clock.pps_locked ||
        !clock.satellites.valid || clock.satellites.used < 4)
        return Health::red;
    return clock.satellites.used == 4 ? Health::amber : Health::green;
}

inline Health health(const presentation::State& state,
                     const ProofOverrides& proof = {}) {
    switch (proof.health) {
    case HealthProof::good: return Health::green;
    case HealthProof::minimum: return Health::amber;
    case HealthProof::bad: return Health::red;
    case HealthProof::live: return liveHealth(state);
    case HealthProof::off: return Health::off;
    }
    return Health::red;
}

inline display_appearance::Rgb healthColor(Health value) {
    switch (value) {
    case Health::off: return {0, 0, 0};
    case Health::green: return display_appearance::health_green;
    case Health::amber: return display_appearance::health_amber;
    case Health::red: return display_appearance::health_red;
    }
    return display_appearance::health_red;
}

inline bool holdoverActive(bool actual_holdover,
                           const ProofOverrides& proof = {}) {
    return proof.holdover == HoldoverProof::on ||
           (proof.holdover == HoldoverProof::live && actual_holdover);
}

// Codes below 3 produced a clunky low-level plateau in physical tests. Collapse
// that part of the cosine envelope to an intentional off dwell, then use codes
// 1..8, shifted down while retaining all eight active levels.
constexpr double holdover_cutoff = 0.5;
constexpr uint8_t holdover_min_intensity = 1;
constexpr uint8_t holdover_max_intensity = 8;
constexpr uint32_t holdover_period_us = 5000000;
constexpr uint32_t holdover_redraw_interval_us = 10000;

class BreathingAnimator {
public:
    void reset(uint32_t now_us) {
        last_us_ = now_us;
        phase_us_ = 0;
        initialized_ = true;
    }
    uint8_t update(uint32_t now_us) {
        if (!initialized_) reset(now_us);
        else {
            // Unsigned subtraction crosses micros() rollover. Calls may skip
            // samples; elapsed time, never call count, advances the phase.
            phase_us_ = static_cast<uint32_t>(
                (static_cast<uint64_t>(phase_us_) + (now_us - last_us_)) %
                holdover_period_us);
            last_us_ = now_us;
        }
        return level();
    }
    double envelope() const {
        return ws2812_intensity::cosineEnvelope(phase_us_, holdover_period_us);
    }
    uint8_t level() const {
        return ws2812_intensity::cutoffChannelCode(envelope(),
            holdover_cutoff, holdover_min_intensity, holdover_max_intensity);
    }
private:
    uint32_t last_us_ = 0;
    uint32_t phase_us_ = 0;
    bool initialized_ = false;
};

struct Frame {
    bool has_day = false;
    uint8_t day = 0;
    Health health_state = Health::red;
    bool holdover = false;
    uint8_t holdover_intensity = 0;
    int8_t level_proof = -1;
};

inline Frame makeFrame(const presentation::State& state,
                       bool actual_holdover, uint8_t holdover_intensity,
                       const ProofOverrides& proof = {}) {
    Frame result;
    result.level_proof = proof.level;
    result.health_state = health(state, proof);
    result.holdover = holdoverActive(actual_holdover, proof);
    result.holdover_intensity = result.holdover ? holdover_intensity : 0;
    if (proof.day != 0) {
        result.has_day = true;
        result.day = proof.day;
    } else if (state.clock.utc_valid) {
        const auto local = presentation::convertUtcForDisplay(
            state.clock.utc, state.zone);
        result.has_day = local.civil.day >= 1 && local.civil.day <= 31;
        result.day = result.has_day ? local.civil.day : 0;
    }
    return result;
}

inline bool sameFrame(const Frame& a, const Frame& b) {
    return a.has_day == b.has_day && a.day == b.day &&
           a.health_state == b.health_state && a.holdover == b.holdover &&
           a.holdover_intensity == b.holdover_intensity &&
           a.level_proof == b.level_proof;
}

// Characterization takes full visual ownership but does not change live
// inputs. Applied before preparing the immutable output stream.
template <typename LogicalFrame>
inline void isolateLevelProof(LogicalFrame& pixels, const Frame& overlay) {
    if (overlay.level_proof >= 0) pixels.clear();
}

inline display_appearance::Rgb outputRed(const Frame& overlay) {
    return {static_cast<uint8_t>(overlay.level_proof >= 0
        ? overlay.level_proof : overlay.holdover_intensity), 0, 0};
}

inline display_appearance::Rgb holdoverColor() {
    return display_appearance::holdover_red;
}

template <typename LogicalFrame>
inline bool apply(LogicalFrame& pixels, const Frame& overlay) {
    bool ok = true;
    if (overlay.has_day) {
        ok = display_compositor::applyOverlayPixel(
            pixels, static_cast<int>(overlay.day) - 1, 7,
            display_compositor::solid(display_appearance::calendar_blue)) && ok;
    }
    ok = display_compositor::applyOverlayPixel(
        pixels, 31, 4,
        display_compositor::solid(healthColor(overlay.health_state))) && ok;
    const auto holdover_paint = overlay.holdover
        ? display_compositor::solid(holdoverColor())
        : display_compositor::solid(display_appearance::Rgb{0, 0, 0});
    ok = display_compositor::applyOverlayPixel(pixels, 31, 3, holdover_paint) && ok;
    return ok;
}

inline bool apply(ws2812_display::Pixels& pixels, const Frame& overlay) {
    return apply<ws2812_display::Pixels>(pixels, overlay);
}
}
