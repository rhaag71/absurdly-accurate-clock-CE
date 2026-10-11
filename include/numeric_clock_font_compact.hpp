#pragma once

#include <cstdint>
#include <limits>

namespace numeric_clock_font_compact {
constexpr int glyph_height = 7;
constexpr int maximum_digit_width = 5;
constexpr int colon_width = 1;
constexpr int default_spacing = 1;

// Hand-drawn, high-contrast masks keep the colon narrow and the "1" compact.
// Row masks are left-aligned within each glyph's declared width.
struct Glyph {
    uint8_t width;
    uint8_t rows[glyph_height];
};

inline const Glyph* lookup(char character) {
    static const Glyph glyphs[] = {
        {5, {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}}, // 0: plain, unslashed
        {3, {0x02, 0x06, 0x02, 0x02, 0x02, 0x02, 0x07}}, // 1
        {5, {0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f}}, // 2
        {5, {0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e}}, // 3
        {5, {0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02}}, // 4
        {5, {0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e}}, // 5
        {5, {0x06, 0x08, 0x10, 0x1e, 0x11, 0x11, 0x0e}}, // 6
        {5, {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}}, // 7
        {5, {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e}}, // 8
        {5, {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x02, 0x0c}}, // 9
        {1, {0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00}}, // :
    };
    if (character >= '0' && character <= '9') return &glyphs[character - '0'];
    return character == ':' ? &glyphs[10] : 0;
}

// Returns the actual bitmap layout width, excluding the unused final gap.
inline int textWidth(const char* text, int spacing = default_spacing) {
    if (!text || spacing < 0) return -1;
    int width = 0;
    for (const char* character = text; *character; ++character) {
        const Glyph* glyph = lookup(*character);
        if (!glyph) return -1;
        if (width > std::numeric_limits<int>::max() - glyph->width) return -1;
        width += glyph->width;
        if (character[1]) {
            if (width > std::numeric_limits<int>::max() - spacing) return -1;
            width += spacing;
        }
    }
    return width;
}

template <typename LogicalFrame, typename Color>
inline bool drawGlyph(LogicalFrame& frame, char character, int x, int y,
                      const Color& color) {
    const Glyph* glyph = lookup(character);
    if (!glyph) return false;
    for (int row = 0; row < glyph_height; ++row) {
        for (int column = 0; column < glyph->width; ++column) {
            if ((glyph->rows[row] & (1u << (glyph->width - 1 - column))) == 0) continue;
            const int64_t px = static_cast<int64_t>(x) + column;
            const int64_t py = static_cast<int64_t>(y) + row;
            if (px >= std::numeric_limits<int>::min() && px <= std::numeric_limits<int>::max() &&
                py >= std::numeric_limits<int>::min() && py <= std::numeric_limits<int>::max())
                frame.setLogical(static_cast<int>(px), static_cast<int>(py), color);
        }
    }
    return true;
}

template <typename LogicalFrame, typename Color>
inline bool drawText(LogicalFrame& frame, const char* text, int x, int y,
                     const Color& color, int spacing = default_spacing) {
    if (!text || spacing < 0) return false;
    int64_t cursor = x;
    for (const char* character = text; *character; ++character) {
        const Glyph* glyph = lookup(*character);
        if (!glyph || cursor < std::numeric_limits<int>::min() ||
            cursor > std::numeric_limits<int>::max()) return false;
        drawGlyph(frame, *character, static_cast<int>(cursor), y, color);
        cursor += glyph->width + (character[1] ? spacing : 0);
    }
    return true;
}

template <typename LogicalFrame, typename Color>
inline bool drawCenteredText(LogicalFrame& frame, const char* text, int panel_width,
                             int y, const Color& color, int spacing = default_spacing) {
    const int width = textWidth(text, spacing);
    if (width < 0 || width > panel_width) return false;
    return drawText(frame, text, (panel_width - width) / 2, y, color, spacing);
}
}
