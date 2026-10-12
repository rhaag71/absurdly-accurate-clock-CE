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
    constexpr uint8_t expected_widths[11] = {5, 3, 5, 5, 5, 5, 5, 5, 5, 5, 1};
    constexpr uint8_t expected_rows[11][7] = {
        {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e},
        {0x02, 0x06, 0x02, 0x02, 0x02, 0x02, 0x07},
        {0x0e, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1f},
        {0x0e, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0e},
        {0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02},
        {0x1f, 0x10, 0x10, 0x1e, 0x01, 0x11, 0x0e},
        {0x0e, 0x11, 0x10, 0x1e, 0x11, 0x11, 0x0e},
        {0x1e, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
        {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e},
        {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x11, 0x0e},
        {0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00},
    };
    assert(glyph_height == 7 && maximum_digit_width == 5);
    assert(colon_width == 1 && default_spacing == 1);
    for (int index = 0; index < 11; ++index) {
        const char character = index < 10 ? static_cast<char>('0' + index) : ':';
        const Glyph* glyph = lookup(character);
        assert(glyph && glyph->width == expected_widths[index]);
        for (int row = 0; row < glyph_height; ++row)
            assert(glyph->rows[row] == expected_rows[index][row]);
    }
    assert(lookup('1')->width == 3 && lookup(':')->width == 1);
    assert(!lookup('A'));

    // Width includes a gap only between glyphs: 1:05 = 3+1+5+5 + 3 gaps.
    assert(textWidth("1:05") == 17);
    assert(textWidth("8:05") == 19);
    assert(textWidth("9:59") == 19);
    assert(textWidth("10:00") == 23);
    assert(textWidth("12:34") == 23);
    assert(textWidth("1") == 3); // No trailing spacing after the last glyph.
    assert(textWidth("12A4") == -1 && textWidth("12:34", -1) == -1);

    constexpr const char* samples[] = {"1:11", "8:05", "9:59", "10:00", "12:34"};
    for (const char* sample : samples) {
        Pixels pixels;
        assert(drawCenteredText(pixels, sample, logical_width, 0, Rgb{255, 96, 0}));
        const int origin = (logical_width - textWidth(sample)) / 2;
        for (int y = 0; y < logical_height; ++y) {
            for (int x = 0; x < logical_width; ++x) {
                if (y == 7 || x < origin || x >= origin + textWidth(sample))
                    assert(!litAt(pixels, x, y));
            }
        }
        assert(litAt(pixels, origin, 0) == ((lookup(sample[0])->rows[0] &
                                            (1u << (lookup(sample[0])->width - 1))) != 0));
    }
    assert((logical_width - textWidth("1:05")) / 2 == 7);
    assert((logical_width - textWidth("8:05")) / 2 == 6);

    Pixels clipped;
    assert(drawGlyph(clipped, '8', logical_width - 1, 0, Rgb{1, 2, 3}));
    assert(drawGlyph(clipped, '0', std::numeric_limits<int>::max(),
                     std::numeric_limits<int>::min(), Rgb{1, 2, 3}));
    assert(!drawCenteredText(clipped, "88:88", 20, 0, Rgb{1, 2, 3}));
}
