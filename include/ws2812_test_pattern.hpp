#pragma once

#include <cstdint>
#include "display_appearance.hpp"
#include "ws2812_pixels.hpp"

namespace ws2812_display {
enum class TestPattern : uint8_t {
    clock, amber, red, green, blue, white, cyan, magenta,
    ramp_red, ramp_green, ramp_blue, ramp_gray,
};

enum class ProofMode : uint8_t { normal, amber, inverted, background };

inline const char* proofModeName(ProofMode mode) {
    switch (mode) {
    case ProofMode::normal: return "normal";
    case ProofMode::amber: return "amber";
    case ProofMode::inverted: return "invert";
    case ProofMode::background: return "bg";
    }
    return "normal";
}

inline display_appearance::Rgb proofBackground(ProofMode mode) {
    switch (mode) {
    case ProofMode::inverted: return display_appearance::soft_white;
    case ProofMode::background: return display_appearance::blue;
    case ProofMode::normal:
    case ProofMode::amber: return {0, 0, 0};
    }
    return {0, 0, 0};
}

inline display_appearance::Rgb proofClockColor(ProofMode mode,
                                               display_appearance::Rgb normal) {
    switch (mode) {
    case ProofMode::normal: return normal;
    case ProofMode::amber: return display_appearance::reference_amber;
    case ProofMode::inverted: return {0, 0, 0};
    case ProofMode::background: return display_appearance::soft_white;
    }
    return normal;
}

inline uint8_t proofBrightness(ProofMode mode, uint8_t normal) {
    return mode == ProofMode::amber
        ? display_appearance::reference_amber_brightness : normal;
}

inline const char* testPatternName(TestPattern pattern) {
    switch (pattern) {
    case TestPattern::clock: return "clock";
    case TestPattern::amber: return "amber";
    case TestPattern::red: return "red";
    case TestPattern::green: return "green";
    case TestPattern::blue: return "blue";
    case TestPattern::white: return "white";
    case TestPattern::cyan: return "cyan";
    case TestPattern::magenta: return "magenta";
    case TestPattern::ramp_red: return "ramp r";
    case TestPattern::ramp_green: return "ramp g";
    case TestPattern::ramp_blue: return "ramp b";
    case TestPattern::ramp_gray: return "ramp gray";
    }
    return "clock";
}

inline bool isRamp(TestPattern pattern) {
    return pattern == TestPattern::ramp_red ||
           pattern == TestPattern::ramp_green ||
           pattern == TestPattern::ramp_blue ||
           pattern == TestPattern::ramp_gray;
}

inline uint8_t testPatternBrightness(TestPattern pattern,
                                     uint8_t configured_brightness) {
    if (pattern == TestPattern::amber)
        return display_appearance::reference_amber_brightness;
    return isRamp(pattern) ? 255 : configured_brightness;
}

inline void renderTestPattern(Pixels& pixels, TestPattern pattern) {
    // A test pattern owns the complete framebuffer. Clearing first makes that
    // contract explicit and prevents any future partial pattern from exposing
    // pixels left by a clock frame or another test pattern.
    pixels.clear();
    Rgb color = {0, 0, 0};
    switch (pattern) {
    case TestPattern::amber: color = display_appearance::paletteColor(
        display_appearance::PaletteColor::amber); break;
    case TestPattern::red: color = display_appearance::paletteColor(
        display_appearance::PaletteColor::red); break;
    case TestPattern::green: color = display_appearance::paletteColor(
        display_appearance::PaletteColor::green); break;
    case TestPattern::blue: color = display_appearance::paletteColor(
        display_appearance::PaletteColor::blue); break;
    case TestPattern::white: color = display_appearance::paletteColor(
        display_appearance::PaletteColor::white); break;
    case TestPattern::cyan: color = display_appearance::paletteColor(
        display_appearance::PaletteColor::cyan); break;
    case TestPattern::magenta: color = display_appearance::paletteColor(
        display_appearance::PaletteColor::magenta); break;
    default: break;
    }
    if (!isRamp(pattern)) {
        pixels.fill(color);
        return;
    }

    // One column per logical input code, 0 through 31, using full brightness
    // during serialization so the low-code steps reach transfer unchanged.
    for (int x = 0; x < logical_width; ++x) {
        const uint8_t value = static_cast<uint8_t>(x);
        Rgb step = {0, 0, 0};
        if (pattern == TestPattern::ramp_red || pattern == TestPattern::ramp_gray)
            step.red = value;
        if (pattern == TestPattern::ramp_green || pattern == TestPattern::ramp_gray)
            step.green = value;
        if (pattern == TestPattern::ramp_blue || pattern == TestPattern::ramp_gray)
            step.blue = value;
        for (int y = 0; y < logical_height; ++y) pixels.setLogical(x, y, step);
    }
}

#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
inline ProofMode& proofModeStorage() {
    static ProofMode mode = ProofMode::normal;
    return mode;
}
inline uint32_t& proofModeGenerationStorage() {
    static uint32_t generation = 0;
    return generation;
}
inline ProofMode proofMode() { return proofModeStorage(); }
inline void setProofMode(ProofMode mode) {
    if (proofModeStorage() == mode) return;
    proofModeStorage() = mode;
    ++proofModeGenerationStorage();
}
inline uint32_t proofModeGeneration() { return proofModeGenerationStorage(); }

class ProofModeRefreshTracker {
public:
    ProofModeRefreshTracker() : observed_(proofModeGeneration()) {}
    void reset() { observed_ = proofModeGeneration(); }
    bool consumeChange() {
        const uint32_t current = proofModeGeneration();
        if (current == observed_) return false;
        observed_ = current;
        return true;
    }
private:
    uint32_t observed_;
};

inline TestPattern& testPatternStorage() {
    static TestPattern pattern = TestPattern::clock;
    return pattern;
}
inline uint32_t& testPatternGenerationStorage() {
    static uint32_t generation = 0;
    return generation;
}
inline TestPattern testPattern() { return testPatternStorage(); }
inline void setTestPattern(TestPattern pattern) {
    if (testPatternStorage() == pattern) return;
    testPatternStorage() = pattern;
    ++testPatternGenerationStorage();
}
inline uint32_t testPatternGeneration() { return testPatternGenerationStorage(); }

class TestPatternRefreshTracker {
public:
    TestPatternRefreshTracker() : observed_(testPatternGeneration()) {}
    void reset() { observed_ = testPatternGeneration(); }
    bool consumeChange() {
        const uint32_t current = testPatternGeneration();
        if (current == observed_) return false;
        observed_ = current;
        return true;
    }
private:
    uint32_t observed_;
};
#endif
}
