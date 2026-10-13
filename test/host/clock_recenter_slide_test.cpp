#include <cassert>

#include "clock_recenter_slide.hpp"
#include "compact_clock_font.hpp"
#include "display_compositor.hpp"
#include "informational_overlay.hpp"
#include "ws2812_pixels.hpp"

namespace {
using display_appearance::Rgb;
using ws2812_display::Pixels;

int centeredX(const char* text) {
    return (ws2812_display::logical_width - compact_clock_font::textWidth(text)) / 2;
}

bool samePixels(const Pixels& left, const Pixels& right) {
    for (std::size_t i = 0; i < ws2812_display::pixel_count; ++i) {
        const Rgb a = left.at(i);
        const Rgb b = right.at(i);
        if (a.red != b.red || a.green != b.green || a.blue != b.blue) return false;
    }
    return true;
}

Rgb logicalPixel(const Pixels& pixels, int x, int y) {
    uint16_t raw = 0;
    assert(ws2812_display::logicalToRaw(x, y, raw));
    return pixels.at(raw);
}
}

int main() {
    using clock_recenter_slide::State;
    using clock_recenter_slide::step_interval_us;
    const Rgb black{0, 0, 0};
    const Rgb white{255, 240, 220};
    Pixels actual, expected, obsolete;

    // Initial valid content has no arbitrary startup origin.
    State slide;
    assert(slide.update(true, centeredX("9:58"), 1000));
    assert(!slide.active() && slide.currentX() == centeredX("9:58"));

    // A new string with the same measured target does not activate movement.
    assert(!slide.update(true, centeredX("9:59"), 2000));
    assert(!slide.active() && slide.currentX() == centeredX("9:59"));
    assert(display_compositor::composeClockAt(actual, "9:59", slide.currentX(), 0,
        black, display_compositor::solid(white)));
    assert(display_compositor::composeClockAt(expected, "9:59", centeredX("9:59"), 0,
        black, display_compositor::solid(white)));
    assert(samePixels(actual, expected));

    // 9:59 -> 10:00 adopts the new content at the old x, then advances one
    // column per complete interval until the actual-width target is reached.
    const int narrow_x = centeredX("9:59");
    const int wide_x = centeredX("10:00");
    assert(narrow_x == 6 && wide_x == 4);
    assert(slide.update(true, wide_x, 3000));
    assert(slide.active() && slide.currentX() == narrow_x && slide.targetX() == wide_x);
    assert(display_compositor::composeClockAt(actual, "10:00", slide.currentX(), 0,
        black, display_compositor::solid(white)));
    assert(display_compositor::composeClockAt(expected, "10:00", narrow_x, 0,
        black, display_compositor::solid(white)));
    assert(display_compositor::composeClockAt(obsolete, "9:59", narrow_x, 0,
        black, display_compositor::solid(white)));
    assert(samePixels(actual, expected) && !samePixels(actual, obsolete));
    assert(!slide.update(true, wide_x, 3000 + step_interval_us - 1));
    assert(slide.currentX() == 6);
    assert(slide.update(true, wide_x, 3000 + step_interval_us));
    assert(slide.currentX() == 5 && slide.active());
    assert(!slide.update(true, wide_x, 3000 + 2 * step_interval_us - 1));
    assert(slide.update(true, wide_x, 3000 + 2 * step_interval_us));
    assert(slide.currentX() == wide_x && !slide.active());

    // 12:59 -> 1:00 moves in the opposite direction and still finishes at
    // the exact measured center.
    slide.reset();
    const int twelve_x = centeredX("12:59");
    const int one_x = centeredX("1:00");
    assert(twelve_x == 4 && one_x == 7);
    slide.update(true, twelve_x, 0);
    assert(slide.update(true, one_x, 10));
    assert(slide.currentX() == 4 && slide.active());
    assert(slide.update(true, one_x, 10 + step_interval_us));
    assert(slide.currentX() == 5);
    assert(slide.update(true, one_x, 10 + 2 * step_interval_us));
    assert(slide.currentX() == 6);
    assert(slide.update(true, one_x, 10 + 3 * step_interval_us));
    assert(slide.currentX() == one_x && !slide.active());

    // Retargeting replaces obsolete content at the current visual position,
    // restarts the interval there, and retains no queue of old targets.
    slide.reset();
    slide.update(true, narrow_x, 100);
    slide.update(true, wide_x, 200);
    slide.update(true, wide_x, 200 + step_interval_us);
    assert(slide.currentX() == 5);
    assert(slide.update(true, one_x, 300 + step_interval_us));
    assert(slide.currentX() == 5 && slide.targetX() == one_x);
    assert(!slide.update(true, one_x, 300 + 2 * step_interval_us - 1));
    assert(slide.update(true, one_x, 300 + 2 * step_interval_us));
    assert(slide.currentX() == 6);
    actual.clear();
    expected.clear();
    obsolete.clear();
    assert(display_compositor::composeClockAt(actual, "1:00", slide.currentX(), 0,
        black, display_compositor::solid(white)));
    assert(display_compositor::composeClockAt(expected, "1:00", 6, 0,
        black, display_compositor::solid(white)));
    assert(display_compositor::composeClockAt(obsolete, "10:00", 6, 0,
        black, display_compositor::solid(white)));
    assert(samePixels(actual, expected) && !samePixels(actual, obsolete));
    assert(slide.update(true, one_x, 300 + 3 * step_interval_us));
    assert(slide.currentX() == one_x && !slide.active());

    // Invalid time clears prior-position history immediately; the next valid
    // frame starts centered rather than sliding from stale placement.
    assert(slide.update(false, 0, 999999));
    assert(!slide.initialized() && !slide.active());
    assert(slide.update(true, wide_x, 1000000));
    assert(slide.currentX() == wide_x && !slide.active());

    // Overlay composition stays later and independent: moving glyphs cannot
    // shift or suppress calendar, health, or holdover pixels.
    informational_overlay::Frame overlay;
    overlay.has_day = true;
    overlay.day = 15;
    overlay.health_state = informational_overlay::Health::green;
    overlay.holdover = true;
    overlay.holdover_intensity = 8;
    for (int x = narrow_x; x >= wide_x; --x) {
        Pixels frame;
        assert(display_compositor::composeClockAt(frame, "10:00", x, 0, black,
            display_compositor::solid(white)));
        assert(informational_overlay::apply(frame, overlay));
        const Rgb day = logicalPixel(frame, 14, 7);
        const Rgb health = logicalPixel(frame, 31, 4);
        const Rgb holdover = logicalPixel(frame, 31, 3);
        assert(day.blue == 255 && day.red == 0 && day.green == 0);
        assert(health.green == 255);
        assert(holdover.red == 255 && holdover.green == 0 && holdover.blue == 0);
    }
}
