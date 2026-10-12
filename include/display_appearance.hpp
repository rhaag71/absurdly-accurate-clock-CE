#pragma once

#include <cstdint>

namespace display_appearance {
struct Rgb {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

// Physical calibration anchor: RGB(255,96,0) at brightness 6 gives the desired
// amber on the BTF 8x32 panel through the installed black grid and diffuser.
// Future gamma/transfer correction must preserve this appearance unless it is
// deliberately re-evaluated on the physical panel.
constexpr Rgb reference_amber = {255, 96, 0};
constexpr uint8_t reference_amber_brightness = 6;

// Everyday default selected after physical evaluation through the current optics.
constexpr Rgb soft_white = {255, 240, 220};
constexpr uint8_t soft_white_brightness = 2;
constexpr Rgb default_clock_color = soft_white;
constexpr uint8_t default_clock_brightness = soft_white_brightness;

// Additional physically evaluated reference/candidate, not the default.
constexpr Rgb neutral_white = {255, 255, 255};
constexpr uint8_t neutral_white_brightness = 3;
constexpr Rgb candidate_soft_white = soft_white;
constexpr uint8_t candidate_soft_white_brightness = soft_white_brightness;

constexpr Rgb red = {255, 0, 0};
constexpr Rgb green = {0, 255, 0};
constexpr Rgb blue = {0, 0, 255};
constexpr Rgb cyan = {0, 255, 255};
constexpr Rgb magenta = {255, 0, 255};

enum class PaletteColor : uint8_t {
    soft_white, white, amber, red, green, blue, cyan, magenta
};

inline Rgb paletteColor(PaletteColor name) {
    switch (name) {
    case PaletteColor::soft_white: return soft_white;
    case PaletteColor::white: return neutral_white;
    case PaletteColor::amber: return reference_amber;
    case PaletteColor::red: return display_appearance::red;
    case PaletteColor::green: return display_appearance::green;
    case PaletteColor::blue: return display_appearance::blue;
    case PaletteColor::cyan: return display_appearance::cyan;
    case PaletteColor::magenta: return display_appearance::magenta;
    }
    return {0, 0, 0};
}
}
