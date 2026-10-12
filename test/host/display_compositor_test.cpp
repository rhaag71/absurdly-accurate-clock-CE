#include <cassert>

#include "display_compositor.hpp"
#include "ws2812_pixels.hpp"

namespace {
bool samePixels(const ws2812_display::Pixels& left,
                const ws2812_display::Pixels& right) {
    for (std::size_t i = 0; i < ws2812_display::pixel_count; ++i) {
        const auto a = left.at(i);
        const auto b = right.at(i);
        if (a.red != b.red || a.green != b.green || a.blue != b.blue) return false;
    }
    return true;
}
}

int main() {
    using display_appearance::Rgb;
    using namespace display_compositor;
    using ws2812_display::Pixels;

    const Rgb black{0, 0, 0};
    const Rgb soft_white{255, 240, 220};
    const Rgb amber{255, 96, 0};
    Pixels actual, expected;

    // Normal composition keeps the approved centered glyph geometry intact.
    assert(composeClock(actual, "8:05", 32, 0, black, solid(soft_white)));
    expected.fill(black);
    assert(compact_clock_font::drawCenteredText(expected, "8:05", 32, 0, soft_white));
    assert(samePixels(actual, expected));

    // Amber changes only paint; mask placement and spacing stay the same.
    assert(composeClock(actual, "8:05", 32, 0, black, solid(amber)));
    expected.fill(black);
    assert(compact_clock_font::drawCenteredText(expected, "8:05", 32, 0, amber));
    assert(samePixels(actual, expected));

    // Inversion is an illuminated background with explicit black glyph paint.
    assert(composeClock(actual, "8:05", 32, 0, soft_white, solid(black)));
    expected.fill(soft_white);
    assert(compact_clock_font::drawCenteredText(expected, "8:05", 32, 0, black));
    assert(samePixels(actual, expected));
    bool found_dark_glyph = false;
    bool found_illuminated_background = false;
    for (std::size_t i = 0; i < ws2812_display::pixel_count; ++i) {
        const auto pixel = actual.at(i);
        if (pixel.red == 0 && pixel.green == 0 && pixel.blue == 0)
            found_dark_glyph = true;
        if (pixel.red == soft_white.red && pixel.green == soft_white.green &&
            pixel.blue == soft_white.blue)
            found_illuminated_background = true;
    }
    assert(found_dark_glyph && found_illuminated_background);

    // Transparent overlay leaves the composed pixel unchanged; opaque black
    // writes dark, and a later opaque paint wins by deterministic layer order.
    const auto amber_overlay = [amber](Pixels& frame) {
        return applyOverlayPixel(frame, 0, 0, solid(amber));
    };
    assert(compose(actual, "1:11", 32, 0, black, solid(soft_white),
                   amber_overlay));
    auto pixel = actual.at(0);
    assert(pixel.red == amber.red && pixel.green == amber.green &&
           pixel.blue == amber.blue);
    assert(applyOverlayPixel(actual, 0, 0, transparent()));
    pixel = actual.at(0);
    assert(pixel.red == amber.red && pixel.green == amber.green &&
           pixel.blue == amber.blue);
    assert(applyOverlayPixel(actual, 0, 0, solid(black)));
    pixel = actual.at(0);
    assert(pixel.red == 0 && pixel.green == 0 && pixel.blue == 0);
    assert(applyOverlayPixel(actual, 0, 0, solid(soft_white)));
    pixel = actual.at(0);
    assert(pixel.red == soft_white.red && pixel.green == soft_white.green &&
           pixel.blue == soft_white.blue);

    // A fully transparent clock paint leaves the background untouched.
    beginFrame(actual, amber);
    assert(drawClockLayer(actual, "12:34", 32, 0, transparent()));
    assert(actual.at(0).red == amber.red && actual.at(0).green == amber.green &&
           actual.at(0).blue == amber.blue);
}
