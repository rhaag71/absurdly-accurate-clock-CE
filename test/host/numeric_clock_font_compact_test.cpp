#include <cassert>
#include <cstdint>
#include <limits>

#include "numeric_clock_font_compact.hpp"
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
    using namespace numeric_clock_font_compact;
    using namespace ws2812_display;
    constexpr uint8_t plain_zero[glyph_height] = {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e};
    constexpr uint8_t expected_widths[10] = {5, 3, 5, 5, 5, 5, 5, 5, 5, 5};
    for (char c = '0'; c <= '9'; ++c) {
        const Glyph* glyph = lookup(c);
        assert(glyph && glyph->width == expected_widths[c - '0'] &&
               glyph->width >= 1 && glyph->width <= maximum_digit_width);
        for (int y = 0; y < glyph_height; ++y)
            assert((glyph->rows[y] & ~((1u << glyph->width) - 1u)) == 0);
    }
    const Glyph* zero = lookup('0');
    for (int y = 0; y < glyph_height; ++y) assert(zero->rows[y] == plain_zero[y]);
    assert(zero->width == 5); // Closed outline, with no diagonal slash.
    assert(lookup('1')->width == 3);
    const Glyph* colon = lookup(':');
    assert(colon && colon->width == colon_width && colon_width == 1);
    for (int y = 0; y < glyph_height; ++y) assert(colon->rows[y] <= 1);
    assert(!lookup('A'));

    constexpr const char* samples[] = {"00:00", "01:11", "08:08", "10:07",
        "11:11", "12:34", "20:08", "23:59", "88:88"};
    for (const char* sample : samples) {
        int sum = 0;
        int count = 0;
        for (const char* c = sample; *c; ++c) { sum += lookup(*c)->width; ++count; }
        assert(textWidth(sample) == sum + (count - 1) * default_spacing);
        assert(textWidth(sample) <= 25 && textWidth(sample) <= logical_width);
    }
    assert(textWidth("88:88") == 25);
    assert(textWidth("11:11") == 17);
    assert(textWidth("12:34") == 23);
    assert(textWidth("12A4") == -1 && textWidth("12:34", -1) == -1);

    uint16_t mapping[logical_height * logical_width];
    for (int y = 0; y < logical_height; ++y)
        for (int x = 0; x < logical_width; ++x)
            assert(logicalToRaw(x, y, mapping[y * logical_width + x]));

    Pixels pixels;
    const Rgb ink{255, 96, 0};
    for (const char* sample : samples) {
        pixels.clear();
        assert(drawCenteredText(pixels, sample, logical_width, 0, ink));
        const int x = (logical_width - textWidth(sample)) / 2;
        const Glyph* first = lookup(sample[0]);
        int first_lit = 0;
        while (first_lit < first->width &&
               !(first->rows[0] & (1u << (first->width - 1 - first_lit)))) ++first_lit;
        if (first_lit < first->width) assert(litAt(pixels, x + first_lit, 0));
        for (int y = 0; y < logical_height; ++y) {
            for (int col = 0; col < logical_width; ++col) {
                if (col < x || col >= x + textWidth(sample)) assert(!litAt(pixels, col, y));
            }
        }
    }
    assert((logical_width - textWidth("88:88")) / 2 == 3);
    assert((logical_width - textWidth("88:88")) - 3 == 4);
    // y=0..6 holds the glyphs; row 7 stays available for later composition.
    pixels.clear();
    assert(drawCenteredText(pixels, "88:88", logical_width, 0, ink));
    bool ink_above = false;
    for (int y = 0; y < glyph_height; ++y)
        for (int x = 0; x < logical_width; ++x) ink_above |= litAt(pixels, x, y);
    assert(ink_above);
    for (int x = 0; x < logical_width; ++x) assert(!litAt(pixels, x, 7));
    pixels.clear();
    assert(drawCenteredText(pixels, "11:11", logical_width, 0, ink));
    assert(litAt(pixels, (logical_width - textWidth("11:11")) / 2 + 1, 0));

    pixels.clear();
    assert(drawGlyph(pixels, '8', logical_width - 1, 1, ink));
    assert(!drawCenteredText(pixels, "88:88", 24, 0, ink));
    assert(drawGlyph(pixels, '0', std::numeric_limits<int>::max(),
                     std::numeric_limits<int>::min(), ink));
    for (int y = 0; y < logical_height; ++y) {
        for (int x = 0; x < logical_width; ++x) {
            uint16_t after = 0xffff;
            assert(logicalToRaw(x, y, after));
            assert(after == mapping[y * logical_width + x]);
            assert(after == static_cast<uint16_t>(x * logical_height +
                (x % 2 == 0 ? y : logical_height - 1 - y)));
        }
    }
}
