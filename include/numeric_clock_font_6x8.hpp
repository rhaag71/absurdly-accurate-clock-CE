#pragma once

#include <cstdint>
#include <limits>

namespace numeric_clock_font_6x8 {
constexpr int glyph_height = 8;
constexpr int digit_width = 6;
constexpr int colon_width = 1;
constexpr int default_spacing = 1;

// Six-bit row masks are drawn for a tall, compact clock face. The narrow
// rounded counters keep adjacent digits distinct through the pixel diffuser.
struct Glyph {
    uint8_t width;
    uint8_t rows[glyph_height];
};

inline const Glyph* lookup(char character) {
    static const Glyph glyphs[] = {
        {6, {0x1e, 0x21, 0x23, 0x25, 0x29, 0x31, 0x21, 0x1e}}, // 0
        {6, {0x0c, 0x1c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x1e}}, // 1
        {6, {0x1e, 0x21, 0x01, 0x02, 0x04, 0x08, 0x10, 0x3f}}, // 2
        {6, {0x3e, 0x01, 0x01, 0x02, 0x0e, 0x01, 0x21, 0x1e}}, // 3
        {6, {0x02, 0x06, 0x0a, 0x12, 0x22, 0x3f, 0x02, 0x02}}, // 4
        {6, {0x3f, 0x20, 0x20, 0x3e, 0x01, 0x01, 0x21, 0x1e}}, // 5
        {6, {0x0e, 0x10, 0x20, 0x3e, 0x21, 0x21, 0x21, 0x1e}}, // 6
        {6, {0x3f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x10, 0x10}}, // 7
        {6, {0x1e, 0x21, 0x21, 0x1e, 0x21, 0x21, 0x21, 0x1e}}, // 8
        {6, {0x1e, 0x21, 0x21, 0x21, 0x1f, 0x01, 0x02, 0x1c}}, // 9
        {1, {0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00}}, // :
    };
    if (character >= '0' && character <= '9') return &glyphs[character - '0'];
    return character == ':' ? &glyphs[10] : 0;
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
        cursor += glyph->width + spacing;
    }
    return true;
}
}
