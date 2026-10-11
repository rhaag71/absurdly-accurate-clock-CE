#pragma once

#include <cstdint>
#include <limits>

// Conventional six-row digits for the 32x8 clock panel. One-pixel strokes
// keep the numerals compact while retaining distinct counters and a readable
// two-bowl 8. Masks are left-aligned inside each glyph's declared width.
namespace compact_clock_font {
constexpr int glyph_height = 6;
constexpr int maximum_digit_width = 4;
constexpr int colon_width = 1;
constexpr int default_spacing = 1;

struct Glyph {
    uint8_t width;
    uint8_t rows[glyph_height];
};

inline const Glyph* lookup(char character) {
    static const Glyph glyphs[] = {
        {4, {0x06, 0x09, 0x09, 0x09, 0x09, 0x06}}, // 0: plain, unslashed
        {2, {0x02, 0x03, 0x02, 0x02, 0x02, 0x03}}, // 1
        {4, {0x06, 0x09, 0x01, 0x02, 0x04, 0x0f}}, // 2
        {4, {0x06, 0x01, 0x01, 0x06, 0x01, 0x06}}, // 3
        {4, {0x09, 0x09, 0x09, 0x0f, 0x01, 0x01}}, // 4
        {4, {0x0f, 0x08, 0x08, 0x0e, 0x01, 0x0e}}, // 5
        {4, {0x06, 0x08, 0x08, 0x0e, 0x09, 0x06}}, // 6
        {4, {0x0f, 0x01, 0x02, 0x04, 0x04, 0x04}}, // 7
        {4, {0x06, 0x09, 0x06, 0x09, 0x09, 0x06}}, // 8: separate bowls and waist
        {4, {0x06, 0x09, 0x09, 0x07, 0x01, 0x06}}, // 9
        {1, {0x00, 0x01, 0x00, 0x00, 0x01, 0x00}}, // :
    };
    if (character >= '0' && character <= '9') return &glyphs[character - '0'];
    return character == ':' ? &glyphs[10] : 0;
}

inline int textWidth(const char* text, int spacing = default_spacing) {
    if (!text || spacing < 0) return -1;
    int width = 0;
    for (const char* character = text; *character; ++character) {
        const Glyph* glyph = lookup(*character);
        if (!glyph || width > std::numeric_limits<int>::max() - glyph->width) return -1;
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
