#include <cassert>
#include <cstdint>
#include <limits>

#include "numeric_clock_font_soft.hpp"
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
    using namespace numeric_clock_font_soft;
    using namespace ws2812_display;
    constexpr uint8_t zero[glyph_height] = {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e};
    constexpr uint8_t two[glyph_height] = {0x0e, 0x11, 0x01, 0x02, 0x04, 0x0c, 0x1e};
    constexpr uint8_t four[glyph_height] = {0x04, 0x0c, 0x14, 0x14, 0x1f, 0x04, 0x04};
    constexpr uint8_t eight[glyph_height] = {0x0e, 0x11, 0x0e, 0x0a, 0x11, 0x11, 0x0e};
    constexpr uint8_t widths[10] = {5, 3, 5, 5, 5, 5, 5, 5, 5, 5};
    for (char digit = '0'; digit <= '9'; ++digit) {
        const Glyph* glyph = lookup(digit);
        assert(glyph && glyph->width == widths[digit - '0']);
        assert(glyph->width >= 1 && glyph->width <= maximum_digit_width);
        for (int row = 0; row < glyph_height; ++row)
            assert((glyph->rows[row] & ~((1u << glyph->width) - 1u)) == 0);
    }
    for (int row = 0; row < glyph_height; ++row) {
        assert(lookup('0')->rows[row] == zero[row]);
        assert(lookup('2')->rows[row] == two[row]);
        assert(lookup('4')->rows[row] == four[row]);
        assert(lookup('8')->rows[row] == eight[row]);
    }
    assert(lookup('1')->width == 3);
    assert(lookup(':') && lookup(':')->width == colon_width && colon_width == 1);
    assert(glyph_height == 7 && !lookup('A'));

    constexpr const char* samples[] = {"1:00", "8:05", "10:07", "11:11",
        "12:34", "20:08", "23:59", "88:88"};
    for (const char* sample : samples) {
        assert(textWidth(sample) >= 0 && textWidth(sample) <= 25);
        Pixels pixels;
        assert(drawCenteredText(pixels, sample, logical_width, 0, Rgb{255, 96, 0}));
        const int origin = (logical_width - textWidth(sample)) / 2;
        for (int y = 0; y < logical_height; ++y)
            for (int x = 0; x < logical_width; ++x)
                if (y == 7 || x < origin || x >= origin + textWidth(sample))
                    assert(!litAt(pixels, x, y));
    }
    assert(textWidth("88:88") == 25);
    assert((logical_width - textWidth("88:88")) / 2 == 3);
    assert(textWidth("12:34", 0) == 19);
    Pixels centered;
    assert(drawCenteredText(centered, "88:88", logical_width, 0, Rgb{1, 2, 3}));
    assert(litAt(centered, 4, 0)); // 25-column text starts at x=3; '8' has a side bearing.
    centered.clear();
    assert(drawCenteredText(centered, "1:00", logical_width, 0, Rgb{1, 2, 3}));
    assert(textWidth("1:00") == 17 && (logical_width - textWidth("1:00")) / 2 == 7);
    assert(litAt(centered, 8, 0)); // Narrow '1' adds its own side bearing to x=7.
    assert(textWidth("12A4") == -1 && textWidth("12:34", -1) == -1);

    Pixels clipped;
    assert(drawGlyph(clipped, '8', logical_width - 1, 1, Rgb{1, 2, 3}));
    assert(drawGlyph(clipped, '0', std::numeric_limits<int>::max(),
                     std::numeric_limits<int>::min(), Rgb{1, 2, 3}));
    assert(!drawCenteredText(clipped, "88:88", 24, 0, Rgb{1, 2, 3}));
}
