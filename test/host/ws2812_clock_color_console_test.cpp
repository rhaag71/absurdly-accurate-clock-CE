#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "dev_console.hpp"
#include "ws2812_brightness.hpp"
#include "ws2812_clock_color.hpp"

namespace {
unsigned status_calls = 0;
unsigned set_calls = 0;
int64_t last_value = -1;
void handler(void*, dev_console::Command command, int64_t value,
             dev_console::Console& console) {
    if (command == dev_console::Command::color_status) {
        ++status_calls;
        const auto color = ws2812_display::clockColor();
        char line[64];
        snprintf(line, sizeof(line), "Logical clock RGB: %u %u %u",
                 static_cast<unsigned>(color.red),
                 static_cast<unsigned>(color.green),
                 static_cast<unsigned>(color.blue));
        console.writeLine(line);
    }
    if (command == dev_console::Command::color_set) {
        ++set_calls;
        last_value = value;
        const uint32_t packed = static_cast<uint32_t>(value);
        ws2812_display::setClockColor({static_cast<uint8_t>(packed >> 16),
            static_cast<uint8_t>(packed >> 8), static_cast<uint8_t>(packed)});
    }
}
void feed(dev_console::Console& console, const char* text) {
    while (*text) console.receive(*text++, handler, nullptr);
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
    assert(clockColor().red == 255 && clockColor().green == 255 && clockColor().blue == 220);
    dev_console::Console console;
    char output[1024];
    feed(console, "\n");
    drain(console, output, sizeof(output));
    feed(console, "color\ncolor 255 96 0\ncolor 255 0 0\ncolor 0 255 0\n"
                  "color 0 0 255\ncolor 0 128 255\ncolor 255 0 128\ncolor 0 0 0\n"
                  "color 255 255 255\n");
    drain(console, output, sizeof(output));
    assert(status_calls == 1 && std::strstr(output, "Logical clock RGB: 255 255 220"));
    assert(set_calls == 8 && last_value == 0xffffff);
    const char* invalid[] = {"color -1 0 0\n", "color 256 0 0\n", "color 0 -1 0\n",
        "color 0 0 256\n", "color 1.0 0 0\n", "color 1 2\n", "color 1 2 3 4\n",
        "color +1 2 3\n", "color 1a 2 3\n", "color 1  2 nope\n",
        "color 999999999999999999999 0 0\n"};
    for (const char* command : invalid) {
        feed(console, command);
        drain(console, output, sizeof(output));
        assert(std::strstr(output, "Usage: color <r> <g> <b> (each 0-255)"));
    }
    assert(set_calls == 8 && status_calls == 1);

    ClockColorRefreshTracker refresh;
    assert(!refresh.consumeChange());
    const uint8_t original_brightness = brightness();
    setBrightness(original_brightness == 255 ? 254 : original_brightness + 1);
    assert(!refresh.consumeChange()); // Brightness does not masquerade as a color change.
    const ClockColor current = clockColor();
    setClockColor({static_cast<uint8_t>(current.red ^ 1u), current.green, current.blue});
    assert(refresh.consumeChange());
    assert(!refresh.consumeChange());
    setBrightness(original_brightness);
    assert(clockColor().red == static_cast<uint8_t>(current.red ^ 1u));
}
