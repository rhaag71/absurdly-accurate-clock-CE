#include <cassert>
#include <cstdint>
#include "numeric_font_5x7.hpp"
#include "ws2812_pixels.hpp"

namespace {
bool litAt(const ws2812_display::Pixels& pixels, int x, int y) {
    uint16_t raw = 0xffff;
    assert(ws2812_display::logicalToRaw(x, y, raw));
    const ws2812_display::Rgb pixel = pixels.at(raw);
    return pixel.red != 0 || pixel.green != 0 || pixel.blue != 0;
}
}

int main() {
    using namespace numeric_font_5x7;
    using namespace ws2812_display;

    for (char digit = '0'; digit <= '9'; ++digit) {
        const Glyph* glyph = lookup(digit);
        assert(glyph && glyph->width == digit_width);
        for (int row = 0; row < glyph_height; ++row)
            assert((glyph->rows[row] & 0xe0u) == 0);
    }
    const Glyph* colon = lookup(':');
    assert(colon && colon->width == 1);
    for (int row = 0; row < glyph_height; ++row)
        assert((colon->rows[row] & 0xfeu) == 0);
    assert(!lookup('A'));

    Pixels pixels;
    const Rgb ink{255, 255, 255};
    assert(drawGlyph(pixels, '2', 4, 0, ink));
    const Glyph* two = lookup('2');
    for (int y = 0; y < logical_height; ++y) {
        for (int x = 0; x < logical_width; ++x) {
            bool expected = false;
            if (x >= 4 && x < 4 + two->width && y < glyph_height)
                expected = (two->rows[y] & (1u << (two->width - 1 - (x - 4)))) != 0;
            assert(litAt(pixels, x, y) == expected);
        }
    }
    assert(!drawGlyph(pixels, 'A', 0, 0, ink));

    pixels.clear();
    assert(drawGlyph(pixels, '8', logical_width - 1, 1, ink));
    const Glyph* eight = lookup('8');
    for (int y = 0; y < logical_height; ++y) {
        for (int x = 0; x < logical_width; ++x) {
            const bool expected = x == logical_width - 1 && y >= 1 &&
                y < 1 + glyph_height &&
                (eight->rows[y - 1] & (1u << (eight->width - 1))) != 0;
            assert(litAt(pixels, x, y) == expected);
        }
    }

    uint16_t mapping_before[pixel_count];
    for (int y = 0; y < logical_height; ++y) {
        for (int x = 0; x < logical_width; ++x) {
            assert(logicalToRaw(x, y, mapping_before[y * logical_width + x]));
        }
    }

    pixels.clear();
    constexpr int text_x = 3;
    constexpr int text_y = 0;
    constexpr char text[] = "12:34";
    constexpr int expected_text_width = 25;
    assert(text_x >= 0 && text_x + expected_text_width <= logical_width);
    assert(text_y >= 0 && text_y + glyph_height <= logical_height);
    assert(drawText(pixels, text, text_x, text_y, ink));

    bool expected_text[logical_height][logical_width] = {};
    int cursor_x = text_x;
    for (const char* character = text; *character; ++character) {
        const Glyph* glyph = lookup(*character);
        assert(glyph);
        for (int row = 0; row < glyph_height; ++row) {
            for (int column = 0; column < glyph->width; ++column) {
                const int x = cursor_x + column;
                const bool expected = (glyph->rows[row] &
                    (1u << (glyph->width - 1 - column))) != 0;
                expected_text[text_y + row][x] = expected;
            }
        }
        cursor_x += glyph->width + default_spacing;
    }
    for (int y = 0; y < logical_height; ++y) {
        for (int x = 0; x < logical_width; ++x) {
            assert(litAt(pixels, x, y) == expected_text[y][x]);
        }
    }

    for (int y = 0; y < logical_height; ++y) {
        for (int x = 0; x < logical_width; ++x) {
            uint16_t raw_after = 0xffff;
            assert(logicalToRaw(x, y, raw_after));
            assert(raw_after == mapping_before[y * logical_width + x]);
        }
    }
}
