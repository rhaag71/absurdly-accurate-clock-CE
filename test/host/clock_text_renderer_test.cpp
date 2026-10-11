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

    // Twelve-hour mode presents natural-width hours before width layout.
    state.clock.utc.hour = 1;
    state.clock.utc.minute = 5;
    state.zone = presentation::DisplayZone::utc;
    assert(clock_text_renderer::formatHHMM(
        state, text, ce_config::HourFormat::twelve_hour));
    assert(std::strcmp(text, "1:05") == 0);
    assert(clock_text_renderer::render(state, pixels, amber,
                                       ce_config::HourFormat::twelve_hour));
    state.zone = presentation::DisplayZone::eastern;
    state.clock.utc.hour = 13;
    state.clock.utc.minute = 5;
    assert(clock_text_renderer::formatHHMM(
        state, text, ce_config::HourFormat::twelve_hour));
    assert(std::strcmp(text, "8:05") == 0);
    assert(clock_text_renderer::render(state, pixels, amber,
                                       ce_config::HourFormat::twelve_hour));
    assert(compact_clock_font::textWidth(text) == 16);
    // "8:05" is 16 columns and receives the integer-centered origin x=8.
    assert(litAt(pixels, 9, 1));
    state.clock.utc.hour = 5;
    state.clock.utc.minute = 0;
    assert(clock_text_renderer::formatHHMM(
        state, text, ce_config::HourFormat::twelve_hour));
    assert(std::strcmp(text, "12:00") == 0); // Eastern midnight is 12 AM.
    state.zone = presentation::DisplayZone::utc;
    state.clock.utc.hour = 23;
    state.clock.utc.minute = 59;
    assert(clock_text_renderer::formatHHMM(
        state, text, ce_config::HourFormat::twelve_hour));
    assert(std::strcmp(text, "11:59") == 0);
    assert(clock_text_renderer::render(state, pixels, amber,
                                       ce_config::HourFormat::twelve_hour));
    state.clock.utc.hour = 12;
    state.clock.utc.minute = 34;
    assert(clock_text_renderer::formatHHMM(
        state, text, ce_config::HourFormat::twelve_hour));
    assert(std::strcmp(text, "12:34") == 0);
    state.clock.utc.hour = 12;
    state.clock.utc.minute = 34;
    state.zone = presentation::DisplayZone::eastern;
    assert(clock_text_renderer::formatHHMM(state, text));
    assert(std::strcmp(text, "07:34") == 0);
    assert(clock_text_renderer::render(state, pixels, amber));

    // The 21-column 07:34 string centers at x=5; row 7 remains unused.
    for (int y = 0; y < ws2812_display::logical_height; ++y) {
        for (int x = 0; x < ws2812_display::logical_width; ++x) {
            if (x < 5 || x >= 26 || y == 0 || y == 7) assert(!litAt(pixels, x, y));
        }
    }
    assert(litAt(pixels, 6, 1)); // First lit stroke of the '0' in 07:34.
    assert(!litAt(pixels, 3, 7));

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
