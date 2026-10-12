#include <cassert>
#include <cstdint>
#include <cstring>

#include "dev_console.hpp"
#include "clock_state.hpp"
#include "display_compositor.hpp"
#include "display_transfer.hpp"
#include "ws2812_brightness.hpp"
#include "ws2812_pixels.hpp"
#include "ws2812_test_pattern.hpp"

namespace {
struct Context {
    unsigned handled = 0;
};

void handle(void* opaque, dev_console::Command command, int64_t value,
            dev_console::Console& console) {
    Context& context = *static_cast<Context*>(opaque);
    ++context.handled;
    if (command == dev_console::Command::transfer_status) {
        console.writeLine(display_appearance::transferModeName(
            display_appearance::transferMode()));
    } else if (command == dev_console::Command::transfer_set) {
        display_appearance::setTransferMode(
            static_cast<display_appearance::TransferMode>(value));
        console.writeLine(display_appearance::transferModeName(
            display_appearance::transferMode()));
    } else if (command == dev_console::Command::pattern_status) {
        console.writeLine(ws2812_display::testPatternName(
            ws2812_display::testPattern()));
    } else if (command == dev_console::Command::presentation_status) {
        console.writeLine(ws2812_display::proofModeName(
            ws2812_display::proofMode()));
    } else if (command == dev_console::Command::presentation_set) {
        ws2812_display::setTestPattern(ws2812_display::TestPattern::clock);
        ws2812_display::setProofMode(
            static_cast<ws2812_display::ProofMode>(value));
        console.writeLine(ws2812_display::proofModeName(
            ws2812_display::proofMode()));
    } else if (command == dev_console::Command::pattern_set ||
               command == dev_console::Command::ramp_set) {
        ws2812_display::setProofMode(ws2812_display::ProofMode::normal);
        ws2812_display::setTestPattern(
            static_cast<ws2812_display::TestPattern>(value));
        console.writeLine(ws2812_display::testPatternName(
            ws2812_display::testPattern()));
    }
}

void feed(dev_console::Console& console, const char* text, Context& context) {
    while (*text) console.receive(*text++, handle, &context);
}

void drain(dev_console::Console& console, char* output, size_t capacity) {
    size_t used = 0;
    char byte;
    while (console.nextOutput(byte)) if (used + 1 < capacity) output[used++] = byte;
    output[used] = '\0';
}
}

int main() {
    using namespace ws2812_display;

    assert(display_appearance::transferMode() ==
           display_appearance::TransferMode::off);
    display_appearance::TransferRefreshTracker transfer_refresh;
    assert(!transfer_refresh.consumeChange());
    display_appearance::setTransferMode(display_appearance::TransferMode::g2);
    assert(transfer_refresh.consumeChange());
    display_appearance::setTransferMode(display_appearance::TransferMode::off);
    assert(transfer_refresh.consumeChange());
    for (unsigned value = 0; value <= 255; ++value) {
        const uint8_t code = static_cast<uint8_t>(value);
        assert(display_appearance::transferChannel(
            code, display_appearance::TransferMode::off) == code);
        const unsigned g2 = display_appearance::transferChannel(
            code, display_appearance::TransferMode::g2);
        const unsigned ws = display_appearance::transferChannel(
            code, display_appearance::TransferMode::ws);
        assert(g2 <= 255 && ws <= 255);
    }
    assert(display_appearance::transferChannel(0, display_appearance::TransferMode::off) == 0);
    assert(display_appearance::transferChannel(0, display_appearance::TransferMode::g2) == 0);
    assert(display_appearance::transferChannel(0, display_appearance::TransferMode::ws) == 0);
    assert(display_appearance::transferChannel(255, display_appearance::TransferMode::g2) == 255);
    assert(display_appearance::transferChannel(255, display_appearance::TransferMode::ws) == 255);
    assert(display_appearance::gamma2(1) == 16 &&
           display_appearance::gamma2(6) == 39 &&
           display_appearance::gamma2(255) == 255);
    assert(display_appearance::ws2812Comparison(1) == 2 &&
           display_appearance::ws2812Comparison(6) == 12 &&
           display_appearance::ws2812Comparison(15) == 29 &&
           display_appearance::ws2812Comparison(255) == 255);
    for (unsigned value = 1; value <= 255; ++value) {
        assert(display_appearance::transferChannel(static_cast<uint8_t>(value),
                   display_appearance::TransferMode::g2) >=
               display_appearance::transferChannel(static_cast<uint8_t>(value - 1),
                   display_appearance::TransferMode::g2));
        assert(display_appearance::transferChannel(static_cast<uint8_t>(value),
                   display_appearance::TransferMode::ws) >=
               display_appearance::transferChannel(static_cast<uint8_t>(value - 1),
                   display_appearance::TransferMode::ws));
    }

    Pixels pixels;
    const Rgb sample{255, 96, 0};
    assert(pixels.set(0, sample));
    const uint32_t off_word = pixels.grbWord(
        0, 6, display_appearance::TransferMode::off);
    assert(off_word == pixels.grbWord(0, 6)); // Exact prior post-brightness path.
    assert(pixels.grbWord(0, 6, display_appearance::TransferMode::g2) != off_word);
    assert(pixels.grbWord(0, 6, display_appearance::TransferMode::ws) != off_word);

    const clock_model::Timebase timebase;
    const bool utc_valid_before = timebase.state().utc_valid;
    TestPatternRefreshTracker pattern_refresh;
    assert(!pattern_refresh.consumeChange());
    setTestPattern(TestPattern::red);
    assert(pattern_refresh.consumeChange());
    setTestPattern(TestPattern::clock);
    assert(pattern_refresh.consumeChange());
    Pixels ramp;
    // Poison the prior frame, then prove a red ramp replaces all 256 pixels.
    ramp.fill({255, 96, 0});
    renderTestPattern(ramp, TestPattern::ramp_red);
    for (int x = 0; x < logical_width; ++x) {
        for (int y = 0; y < logical_height; ++y) {
            uint16_t raw = 0xffff;
            assert(logicalToRaw(x, y, raw));
            assert(raw < pixel_count);
            const Rgb pixel = ramp.at(raw);
            assert(pixel.red == x && pixel.green == 0 && pixel.blue == 0);
            // Serialization preserves exactly one transfer application and
            // keeps the other two channels dark (no amber/color leakage).
            const uint32_t off_word = ramp.grbWord(
                raw, testPatternBrightness(TestPattern::ramp_red, 6),
                display_appearance::TransferMode::off);
            assert(off_word == (static_cast<uint32_t>(x) << 16));
            const uint32_t g2_word = ramp.grbWord(
                raw, 255, display_appearance::TransferMode::g2);
            assert(g2_word == (static_cast<uint32_t>(display_appearance::gamma2(
                static_cast<uint8_t>(x))) << 16));
            const uint32_t ws_word = ramp.grbWord(
                raw, 255, display_appearance::TransferMode::ws);
            assert(ws_word == (static_cast<uint32_t>(display_appearance::ws2812Comparison(
                static_cast<uint8_t>(x))) << 16));
        }
    }
    uint16_t out_of_bounds_raw = 0xffff;
    assert(!logicalToRaw(logical_width, 0, out_of_bounds_raw));

    const TestPattern ramps[] = {TestPattern::ramp_green, TestPattern::ramp_blue,
                                 TestPattern::ramp_gray};
    for (TestPattern pattern : ramps) {
        renderTestPattern(ramp, pattern);
        for (int x = 0; x < logical_width; ++x) {
            for (int y = 0; y < logical_height; ++y) {
                uint16_t raw = 0xffff;
                assert(logicalToRaw(x, y, raw));
                const Rgb pixel = ramp.at(raw);
                const uint8_t expected_red = pattern == TestPattern::ramp_gray ? x : 0;
                const uint8_t expected_green = pattern == TestPattern::ramp_green ||
                    pattern == TestPattern::ramp_gray ? x : 0;
                const uint8_t expected_blue = pattern == TestPattern::ramp_blue ||
                    pattern == TestPattern::ramp_gray ? x : 0;
                assert(pixel.red == expected_red && pixel.green == expected_green &&
                       pixel.blue == expected_blue);
            }
        }
    }
    uint16_t raw = 0xffff;
    assert(logicalToRaw(0, 0, raw));
    assert(ramp.at(raw).red == 0 && ramp.at(raw).green == 0 && ramp.at(raw).blue == 0);
    assert(logicalToRaw(31, 0, raw));
    assert(ramp.at(raw).red == 31 && ramp.at(raw).green == 31 && ramp.at(raw).blue == 31);
    assert(!logicalToRaw(logical_width, 0, raw));
    assert(timebase.state().utc_valid == utc_valid_before);
    Pixels proof;
    const Rgb default_soft_white = display_appearance::default_clock_color;
    assert(display_compositor::composeClock(proof, "8:05", logical_width, 0,
                        proofBackground(ProofMode::normal),
                        display_compositor::solid(
                            proofClockColor(ProofMode::normal, default_soft_white))));
    assert(proof.at(0).red == 0 && proof.at(0).green == 0 && proof.at(0).blue == 0);
    assert(proofClockColor(ProofMode::amber, default_soft_white).green == 96);
    assert(proofBrightness(ProofMode::amber, 2) == 6);
    assert(proofBackground(ProofMode::inverted).red == 255);
    assert(proofClockColor(ProofMode::inverted, default_soft_white).red == 0);
    assert(display_compositor::composeClock(proof, "8:05", logical_width, 0,
                        proofBackground(ProofMode::background),
                        display_compositor::solid(
                            proofClockColor(ProofMode::background,
                                            default_soft_white))));
    assert(proof.at(0).red == 0 && proof.at(0).green == 0 && proof.at(0).blue == 255);
    renderTestPattern(ramp, TestPattern::amber);
    assert(ramp.at(0).red == 255 && ramp.at(0).green == 96 && ramp.at(0).blue == 0);
    assert(testPatternBrightness(TestPattern::amber, 200) == 6);
    assert(testPatternBrightness(TestPattern::ramp_gray, 6) == 255);
    assert(testPatternBrightness(TestPattern::red, 21) == 21);
    renderTestPattern(ramp, TestPattern::cyan);
    assert(ramp.at(0).red == 0 && ramp.at(0).green == 255 && ramp.at(0).blue == 255);
    const TestPattern solid_patterns[] = {
        TestPattern::red, TestPattern::green, TestPattern::blue,
        TestPattern::white, TestPattern::cyan, TestPattern::magenta,
    };
    const Rgb solid_colors[] = {
        {255, 0, 0}, {0, 255, 0}, {0, 0, 255},
        {255, 255, 255}, {0, 255, 255}, {255, 0, 255},
    };
    for (unsigned i = 0; i < sizeof(solid_patterns) / sizeof(solid_patterns[0]); ++i) {
        renderTestPattern(ramp, solid_patterns[i]);
        const Rgb rendered = ramp.at(0);
        assert(rendered.red == solid_colors[i].red &&
               rendered.green == solid_colors[i].green &&
               rendered.blue == solid_colors[i].blue);
    }

    dev_console::Console console;
    Context context;
    char output[1024];
    feed(console, "\n", context);
    drain(console, output, sizeof(output));
    feed(console, "g\ng g2\ng ws\ng off\npat red\npat green\npat blue\n"
                  "pat white\npat cyan\npat magenta\nramp gray\npat off\npat\n"
                  "p\np norm\np amber\np invert\np bg\np norm\n",
         context);
    drain(console, output, sizeof(output));
    assert(std::strstr(output, "off") && std::strstr(output, "g2") &&
           std::strstr(output, "ws") && std::strstr(output, "ramp gray") &&
           std::strstr(output, "green") && std::strstr(output, "blue") &&
           std::strstr(output, "white") && std::strstr(output, "cyan") &&
           std::strstr(output, "magenta"));
    assert(display_appearance::transferMode() ==
           display_appearance::TransferMode::off);
    assert(testPattern() == TestPattern::clock);
    assert(proofMode() == ProofMode::normal);
    const unsigned handled = context.handled;
    feed(console, "g nope\npat rainbow\nramp x\np rainbow\n", context);
    drain(console, output, sizeof(output));
    assert(std::strstr(output, "Usage: g {off|g2|ws}"));
    assert(std::strstr(output, "Usage: pat {amber|r|g|b|w|c|m|off}"));
    assert(std::strstr(output, "Usage: ramp {r|g|b|gray}"));
    assert(std::strstr(output, "Usage: p {norm|amber|invert|bg}"));
    assert(context.handled == handled);
    assert(timebase.state().utc_valid == utc_valid_before);
}
