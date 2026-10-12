#pragma once

#include <cstddef>
#include <cstdint>
#include "display_transfer.hpp"
#include "ws2812_brightness.hpp"

namespace ws2812_display {
constexpr std::size_t pixel_count = 256;
constexpr int logical_width = 32;
constexpr int logical_height = 8;
constexpr std::size_t perimeter_pixel_count =
    2 * logical_width + 2 * logical_height - 4;

// Physically observed panel orientation: raw zero is top-left, and the chain
// snakes down/up each column while columns progress from left to right.
enum class Origin : uint8_t { top_left, top_right, bottom_left, bottom_right };
struct Mapping {
    Origin origin;
    bool serpentine_columns;
    bool first_column_top_to_bottom;
};
constexpr Mapping panel_mapping = {Origin::top_left, true, true};

inline bool logicalToRaw(int x, int y, uint16_t& raw_index) {
    if (x < 0 || x >= logical_width || y < 0 || y >= logical_height) return false;

    const bool origin_right = panel_mapping.origin == Origin::top_right ||
                              panel_mapping.origin == Origin::bottom_right;
    const bool origin_bottom = panel_mapping.origin == Origin::bottom_left ||
                               panel_mapping.origin == Origin::bottom_right;
    const unsigned row = static_cast<unsigned>(origin_bottom
        ? logical_height - 1 - y : y);
    const unsigned column = static_cast<unsigned>(origin_right
        ? logical_width - 1 - x : x);
    const bool forward = panel_mapping.first_column_top_to_bottom !=
                         (panel_mapping.serpentine_columns && (column & 1u));
    const unsigned wire_row = forward ? row : logical_height - 1 - row;
    const unsigned index = column * logical_height + wire_row;
    if (index >= pixel_count) return false;
    raw_index = static_cast<uint16_t>(index);
    return true;
}

// Clockwise border order: full top edge, right edge below its top corner,
// bottom edge leftward without its right corner, then left edge upward without
// either corner. This yields each of the 76 border pixels exactly once.
inline bool perimeterStepToXY(std::size_t step, int& x, int& y) {
    if (step >= perimeter_pixel_count) return false;

    if (step < static_cast<std::size_t>(logical_width)) {
        x = static_cast<int>(step);
        y = 0;
    } else if (step < static_cast<std::size_t>(logical_width + logical_height - 1)) {
        x = logical_width - 1;
        y = static_cast<int>(step) - (logical_width - 1);
    } else if (step < static_cast<std::size_t>(2 * logical_width + logical_height - 2)) {
        x = 2 * logical_width + logical_height - 3 - static_cast<int>(step);
        y = logical_height - 1;
    } else {
        x = 0;
        y = static_cast<int>(perimeter_pixel_count - step);
    }
    return true;
}

using Rgb = display_appearance::Rgb;

// Raw wire-order RGB storage. Brightness is applied only while serializing,
// so the stored color values remain unchanged.
class Pixels {
public:
    Pixels() { clear(); }

    void clear() {
        for (std::size_t i = 0; i < pixel_count; ++i) pixels_[i] = {0, 0, 0};
    }

    bool set(std::size_t raw_index, Rgb color) {
        if (raw_index >= pixel_count) return false;
        pixels_[raw_index] = color;
        return true;
    }

    bool setLogical(int x, int y, Rgb color) {
        uint16_t raw_index;
        return logicalToRaw(x, y, raw_index) && set(raw_index, color);
    }

    void fill(Rgb color) {
        for (std::size_t i = 0; i < pixel_count; ++i) pixels_[i] = color;
    }

    Rgb at(std::size_t raw_index) const {
        return raw_index < pixel_count ? pixels_[raw_index] : Rgb{0, 0, 0};
    }

    // WS2812 transmits green, red, blue, each most-significant bit first.
    uint32_t grbWord(std::size_t raw_index, uint8_t brightness = 255,
                     display_appearance::TransferMode transfer =
                         display_appearance::TransferMode::off) const {
        if (raw_index >= pixel_count) return 0;
        return grbWordForColor(pixels_[raw_index], brightness, transfer);
    }

    uint32_t grbWordForColor(Rgb pixel, uint8_t brightness,
                             display_appearance::TransferMode transfer) const {
        const uint32_t green = display_appearance::transferChannel(
            scaleChannel(pixel.green, brightness), transfer);
        const uint32_t red = display_appearance::transferChannel(
            scaleChannel(pixel.red, brightness), transfer);
        const uint32_t blue = display_appearance::transferChannel(
            scaleChannel(pixel.blue, brightness), transfer);
        return (green << 24) | (red << 16) | (blue << 8);
    }

private:
    Rgb pixels_[pixel_count];
};

inline bool setLogicalRowFrame(Pixels& pixels, int y, Rgb color) {
    if (y < 0 || y >= logical_height) return false;
    pixels.clear();
    for (int x = 0; x < logical_width; ++x)
        pixels.setLogical(x, y, color);
    return true;
}

inline bool setLogicalColumnFrame(Pixels& pixels, int x, Rgb color) {
    if (x < 0 || x >= logical_width) return false;
    pixels.clear();
    for (int y = 0; y < logical_height; ++y)
        pixels.setLogical(x, y, color);
    return true;
}

static_assert(sizeof(Pixels) == pixel_count * sizeof(Rgb),
              "WS2812 pixel storage must remain exactly 768 bytes");
}
