#include <cassert>
#include <cstdint>
#include <limits>

#include "compact_clock_font.hpp"
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
    using namespace compact_clock_font;
    using namespace ws2812_display;
    constexpr uint8_t zero[glyph_height] = {0x06, 0x09, 0x09, 0x09, 0x09, 0x06};
    constexpr uint8_t one[glyph_height] = {0x02, 0x03, 0x02, 0x02, 0x02, 0x03};
    constexpr uint8_t two[glyph_height] = {0x06, 0x09, 0x01, 0x02, 0x04, 0x0f};
    constexpr uint8_t three[glyph_height] = {0x06, 0x01, 0x01, 0x06, 0x01, 0x06};
    constexpr uint8_t eight[glyph_height] = {0x06, 0x09, 0x06, 0x09, 0x09, 0x06};
    constexpr uint8_t colon[glyph_height] = {0x00, 0x01, 0x00, 0x00, 0x01, 0x00};
    constexpr uint8_t widths[10] = {4, 2, 4, 4, 4, 4, 4, 4, 4, 4};

    for (char digit = '0'; digit <= '9'; ++digit) {
        const Glyph* glyph = lookup(digit);
        assert(glyph && glyph->width == widths[digit - '0']);
        assert(glyph->width >= 1 && glyph->width <= maximum_digit_width);
        assert(glyph_height == 6);
        for (int row = 0; row < glyph_height; ++row)
            assert((glyph->rows[row] & ~((1u << glyph->width) - 1u)) == 0);
    }
    for (int row = 0; row < glyph_height; ++row) {
        assert(lookup('0')->rows[row] == zero[row]);
        assert(lookup('1')->rows[row] == one[row]);
        assert(lookup('2')->rows[row] == two[row]);
        assert(lookup('3')->rows[row] == three[row]);
        assert(lookup('8')->rows[row] == eight[row]);
        assert(lookup(':')->rows[row] == colon[row]);
    }
    assert(lookup('0')->width == 4); // Plain outline, with no slash or dot.
    assert(lookup('1')->width == 2);
    assert(lookup(':')->width == colon_width && colon_width == 1);
    // The 8 has separate upper/lower bowl outlines connected by its middle row.
    assert(lookup('8')->rows[0] == 0x06 && lookup('8')->rows[2] == 0x06 &&
           lookup('8')->rows[3] == 0x09 && lookup('8')->rows[5] == 0x06);
    assert(!lookup('A'));

    constexpr const char* samples[] = {
        "1:05", "8:05", "10:07", "11:59", "12:34", "20:08", "23:59", "88:88",
    };
    for (const char* sample : samples) {
        assert(textWidth(sample) > 0 && textWidth(sample) <= 21);
        Pixels pixels;
        assert(drawCenteredText(pixels, sample, logical_width, 1, Rgb{255, 96, 0}));
        const int origin = (logical_width - textWidth(sample)) / 2;
        for (int y = 0; y < logical_height; ++y) {
            for (int x = 0; x < logical_width; ++x) {
                if (y == 0 || y == 7 || x < origin || x >= origin + textWidth(sample))
                    assert(!litAt(pixels, x, y));
            }
        }
    }
    assert(textWidth("88:88") == 21);
    assert((logical_width - textWidth("88:88")) / 2 == 5);
    assert(textWidth("1:05") == 14);
    assert((logical_width - textWidth("1:05")) / 2 == 9);
    assert(textWidth("12A4") == -1 && textWidth("12:34", -1) == -1);

    Pixels clipped;
    assert(drawGlyph(clipped, '8', logical_width - 1, 1, Rgb{1, 2, 3}));
    assert(drawGlyph(clipped, '0', std::numeric_limits<int>::max(),
                     std::numeric_limits<int>::min(), Rgb{1, 2, 3}));
    assert(!drawCenteredText(clipped, "88:88", 20, 0, Rgb{1, 2, 3}));
}
