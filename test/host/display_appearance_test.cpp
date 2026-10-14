#include <cassert>

#include "display_appearance.hpp"
#include "display_transfer.hpp"
#include "ws2812_brightness.hpp"
#include "ws2812_clock_color.hpp"

int main() {
    using namespace display_appearance;
    assert(reference_amber.red == 255);
    assert(reference_amber.green == 96);
    assert(reference_amber.blue == 0);
    assert(reference_amber_brightness == 6);
    assert(neutral_white.red == 255 && neutral_white.green == 255 &&
           neutral_white.blue == 255);
    assert(neutral_white_brightness == 3);
    assert(candidate_soft_white.red == 255 && candidate_soft_white.green == 240 &&
           candidate_soft_white.blue == 220);
    assert(candidate_soft_white_brightness == 2);

    const ws2812_display::ClockColor default_color = ws2812_display::default_clock_color;
    assert(default_color.red == 255 && default_color.green == 255 &&
           default_color.blue == 220);
    assert(ws2812_display::clockColor().red == 255);
    assert(ws2812_display::clockColor().green == 255);
    assert(ws2812_display::clockColor().blue == 220);
    assert(ws2812_display::default_brightness == normal_clock_brightness);
    assert(ws2812_display::brightness() == normal_clock_brightness);

    const Rgb expected_palette[] = {
        {255, 240, 220}, {255, 255, 255}, {255, 96, 0}, {255, 0, 0},
        {0, 255, 0}, {0, 0, 255}, {0, 255, 255}, {255, 0, 255},
    };
    for (unsigned i = 0; i < sizeof(expected_palette) / sizeof(expected_palette[0]); ++i) {
        const Rgb actual = paletteColor(static_cast<PaletteColor>(i));
        assert(actual.red == expected_palette[i].red &&
               actual.green == expected_palette[i].green &&
               actual.blue == expected_palette[i].blue);
    }
    assert(transferMode() == TransferMode::off);
}
