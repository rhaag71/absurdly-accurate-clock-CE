#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>

#include "numeric_clock_font_6x8.hpp"
#include "ws2812_pixels.hpp"

namespace {
bool litAt(const ws2812_display::Pixels& pixels, int x, int y) {
    uint16_t raw = 0xffff;
    assert(ws2812_display::logicalToRaw(x, y, raw));
    const ws2812_display::Rgb pixel = pixels.at(raw);
    return pixel.red || pixel.green || pixel.blue;
}
}

int main() {
    using namespace numeric_clock_font_6x8;
    using namespace ws2812_display;
    for (char c = '0'; c <= '9'; ++c) {
        const Glyph* glyph = lookup(c);
        assert(glyph && glyph->width == digit_width && digit_width <= 6);
        for (int y = 0; y < glyph_height; ++y) assert((glyph->rows[y] & 0xc0u) == 0);
    }
    const Glyph* colon = lookup(':');
    assert(colon && colon->width == colon_width && colon->width == 1);
    for (int y = 0; y < glyph_height; ++y) assert((colon->rows[y] & 0xfeu) == 0);
    assert(glyph_height == 8 && !lookup('A'));

    // Guard distinctive hand-drawn row masks against accidental edits.
    assert(lookup('0')->rows[0] == 0x1e && lookup('0')->rows[7] == 0x1e);
    assert(lookup('1')->rows[0] == 0x0c && lookup('1')->rows[7] == 0x1e);
    assert(lookup('2')->rows[0] == 0x1e && lookup('2')->rows[7] == 0x3f);
    assert(lookup('8')->rows[0] == 0x1e && lookup('8')->rows[7] == 0x1e);

    constexpr const char* samples[] = {"00:00", "01:11", "08:08", "10:07",
        "11:11", "12:34", "20:08", "23:59"};
    for (const char* sample : samples) {
        int width = 0;
        for (const char* c = sample; *c; ++c) width += lookup(*c)->width;
        width += 4 * default_spacing;
        assert(width == 29 && width <= logical_width);
    }

    Pixels pixels;
    const Rgb ink{255, 96, 0};
    assert(drawText(pixels, "12:34", 1, 0, ink));
    assert(litAt(pixels, 1, 0) == ((lookup('1')->rows[0] & 0x20u) != 0));
    // The final glyph reaches row 7; no bottom row is reserved.
    bool bottom_row_lit = false;
    for (int x = 0; x < logical_width; ++x) bottom_row_lit |= litAt(pixels, x, 7);
    assert(bottom_row_lit);
    for (int y = 0; y < logical_height; ++y)
        for (int x = 0; x < logical_width; ++x)
            if (x < 1 || x >= 30) assert(!litAt(pixels, x, y));

    pixels.clear();
    assert(drawGlyph(pixels, '8', logical_width - 1, 1, ink));
    for (int y = 0; y < logical_height; ++y)
        for (int x = 0; x < logical_width; ++x)
            assert(litAt(pixels, x, y) == (x == logical_width - 1 && y >= 1 &&
                y < logical_height && (lookup('8')->rows[y - 1] & 0x20u)));
    pixels.clear();
    assert(drawGlyph(pixels, '0', std::numeric_limits<int>::max(),
                     std::numeric_limits<int>::min(), ink));
    assert(drawGlyph(pixels, '0', std::numeric_limits<int>::min(),
                     std::numeric_limits<int>::max(), ink));
    assert(!drawText(pixels, "12A", 0, 0, ink));
}
