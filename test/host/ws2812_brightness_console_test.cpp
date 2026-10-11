#include <cassert>
#include <cstdint>
#include <cstring>
#include "dev_console.hpp"

namespace {
unsigned status_calls = 0;
unsigned set_calls = 0;
int64_t last_value = -1;

void handler(void*, dev_console::Command command, int64_t value,
             dev_console::Console&) {
    if (command == dev_console::Command::brightness_status) ++status_calls;
    if (command == dev_console::Command::brightness_set) {
        ++set_calls;
        last_value = value;
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
    dev_console::Console console;
    char output[1024];
    feed(console, "\n");
    drain(console, output, sizeof(output));
    feed(console, "brightness\nbrightness 0\nbrightness 255\n");
    drain(console, output, sizeof(output));
    assert(status_calls == 1 && set_calls == 2 && last_value == 255);

    const char* invalid[] = {"brightness -1\n", "brightness +1\n",
        "brightness 256\n", "brightness 1.5\n", "brightness 1 extra\n"};
    for (const char* command : invalid) {
        feed(console, command);
        drain(console, output, sizeof(output));
        assert(std::strstr(output, "Usage: brightness <0-255>") != nullptr);
    }
    assert(set_calls == 2 && status_calls == 1);
}
