#include <cassert>
#include <cstring>

#include "clock_text_renderer.hpp"
#include "ws2812_pixels.hpp"

namespace {
bool litAt(const ws2812_display::Pixels& pixels, int x, int y) {
    uint16_t raw = 0xffff;
    assert(ws2812_display::logicalToRaw(x, y, raw));
    const ws2812_display::Rgb p = pixels.at(raw);
    return p.red || p.green || p.blue;
}
}

int main() {
    presentation::State state;
    ws2812_display::Pixels pixels;
    const ws2812_display::Rgb amber{255, 96, 0};
    char text[6] = {};

    // No receiver-derived UTC means no displayed time.
    assert(!clock_text_renderer::formatHHMM(state, text));
    assert(!clock_text_renderer::render(state, pixels, amber));
    for (int y = 0; y < ws2812_display::logical_height; ++y)
        for (int x = 0; x < ws2812_display::logical_width; ++x)
            assert(!litAt(pixels, x, y));

    state.clock.utc_valid = true;
    state.clock.utc.year = 2026;
    state.clock.utc.month = 1;
    state.clock.utc.day = 1;
    state.clock.utc.hour = 12;
    state.clock.utc.minute = 34;
    state.zone = presentation::DisplayZone::eastern;
    assert(clock_text_renderer::formatHHMM(state, text));
    assert(std::strcmp(text, "07:34") == 0); // Civil zone conversion is reused.
    assert(clock_text_renderer::render(state, pixels, amber));

    // The fixed five-glyph layout begins at x=3 and occupies rows 0..6.
    for (int y = 0; y < ws2812_display::logical_height; ++y) {
        for (int x = 0; x < ws2812_display::logical_width; ++x) {
            if (y == 7 || x < 3 || x >= 28) assert(!litAt(pixels, x, y));
        }
    }
    assert(litAt(pixels, 4, 0)); // First lit stroke of the '0' in 07:34.

    bool before_minute_change[ws2812_display::logical_height]
                             [ws2812_display::logical_width] = {};
    for (int y = 0; y < ws2812_display::logical_height; ++y)
        for (int x = 0; x < ws2812_display::logical_width; ++x)
            before_minute_change[y][x] = litAt(pixels, x, y);

    state.clock.utc.minute = 35;
    assert(clock_text_renderer::formatHHMM(state, text));
    assert(std::strcmp(text, "07:35") == 0);
    assert(clock_text_renderer::render(state, pixels, amber));
    bool changed = false;
    for (int y = 0; y < ws2812_display::logical_height; ++y) {
        for (int x = 0; x < ws2812_display::logical_width; ++x) {
            if (before_minute_change[y][x] != litAt(pixels, x, y)) changed = true;
        }
    }
    assert(changed);

    // Logical rendering never changes the verified logical-to-raw map.
    uint16_t raw = 0xffff;
    assert(ws2812_display::logicalToRaw(0, 0, raw) && raw == 0);
    assert(ws2812_display::logicalToRaw(31, 7, raw) && raw == 248);
}
