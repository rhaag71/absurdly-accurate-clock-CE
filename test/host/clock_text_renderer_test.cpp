#include <cassert>
#include <cstring>

#include "clock_text_renderer.hpp"
#include "ws2812_pixels.hpp"

namespace {
bool samePixels(const ws2812_display::Pixels& a, const ws2812_display::Pixels& b) {
    for (std::size_t i = 0; i < ws2812_display::pixel_count; ++i) {
        const ws2812_display::Rgb x = a.at(i), y = b.at(i);
        if (x.red != y.red || x.green != y.green || x.blue != y.blue) return false;
    }
    return true;
}
}

int main() {
    presentation::State state;
    ws2812_display::Pixels pixels;
    const ws2812_display::Rgb amber{255, 96, 0};
    char text[6] = {};

    assert(!clock_text_renderer::formatHHMM(state, text));
    assert(!clock_text_renderer::render(state, pixels, amber));

    state.clock.utc_valid = true;
    state.clock.utc.year = 2026;
    state.clock.utc.month = 1;
    state.clock.utc.day = 1;
    state.zone = presentation::DisplayZone::utc;

    // Every possible UTC hour is formatted through the physical face's fixed
    // 12-hour rule; the selected compile-time preference cannot enable 24h.
    const char* expected_hours[24] = {
        "12:34", "1:34", "2:34", "3:34", "4:34", "5:34", "6:34", "7:34",
        "8:34", "9:34", "10:34", "11:34", "12:34", "1:34", "2:34", "3:34",
        "4:34", "5:34", "6:34", "7:34", "8:34", "9:34", "10:34", "11:34",
    };
    state.clock.utc.minute = 34;
    for (unsigned hour = 0; hour < 24; ++hour) {
        state.clock.utc.hour = hour;
        assert(clock_text_renderer::formatHHMM(state, text));
        assert(std::strcmp(text, expected_hours[hour]) == 0);
        assert(clock_text_renderer::render(state, pixels, amber));
        ws2812_display::Pixels expected;
        assert(compact_clock_font::drawCenteredText(expected, text, 32, 0, amber));
        assert(samePixels(pixels, expected));
    }

    state.clock.utc.hour = 8;
    state.clock.utc.minute = 5;
    assert(clock_text_renderer::formatHHMM(state, text));
    assert(std::strcmp(text, "8:05") == 0);
    assert(clock_text_renderer::render(state, pixels, amber));
    ws2812_display::Pixels expected;
    assert(compact_clock_font::textWidth(text) == 19);
    assert(compact_clock_font::drawCenteredText(expected, "8:05", 32, 0, amber));
    assert(samePixels(pixels, expected));

    state.clock.utc.hour = 1;
    assert(clock_text_renderer::formatHHMM(state, text));
    assert(std::strcmp(text, "1:05") == 0);
    assert(compact_clock_font::textWidth(text) == 17);
    assert(clock_text_renderer::render(state, pixels, amber));
    expected.clear();
    assert(compact_clock_font::drawCenteredText(expected, "1:05", 32, 0, amber));
    assert(samePixels(pixels, expected));

    // The origin shifts with the exact width; an odd spare column is assigned
    // deterministically to the right side by integer division.
    assert((32 - compact_clock_font::textWidth("1:05")) / 2 == 7);
    assert((32 - compact_clock_font::textWidth("8:05")) / 2 == 6);

    // Midnight and noon both display 12. A civil offset preserves the same rule.
    state.clock.utc.hour = 0;
    state.clock.utc.minute = 0;
    assert(clock_text_renderer::formatHHMM(state, text));
    assert(std::strcmp(text, "12:00") == 0);
    state.clock.utc.hour = 12;
    assert(clock_text_renderer::formatHHMM(state, text));
    assert(std::strcmp(text, "12:00") == 0);
    state.zone = presentation::DisplayZone::eastern;
    state.clock.utc.hour = 5; // Midnight EST on the prior civil date.
    state.clock.utc.day = 2;
    assert(clock_text_renderer::formatHHMM(state, text));
    assert(std::strcmp(text, "12:00") == 0);
}
