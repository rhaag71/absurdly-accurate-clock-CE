#include <cassert>
#include <cstdint>
#include <cstring>
#include "dev_console.hpp"

namespace {
struct Calls {
    unsigned status = 0;
    unsigned reset = 0;
    unsigned set_time = 0;
    int64_t epoch = 0;
};

void handler(void* opaque, dev_console::Command command, int64_t epoch,
             dev_console::Console& console) {
    auto& calls = *static_cast<Calls*>(opaque);
    if (command == dev_console::Command::status) {
        ++calls.status;
        console.writeLine("STATUS RESPONSE");
    } else if (command == dev_console::Command::reset) {
        ++calls.reset;
        console.writeLine("RESET RESPONSE");
    } else {
        ++calls.set_time;
        calls.epoch = epoch;
        console.writeLine("TIME RESPONSE");
    }
}

void feed(dev_console::Console& console, const char* text, Calls& calls) {
    while (*text) console.receive(*text++, handler, &calls);
}

void drain(dev_console::Console& console, char* output, size_t capacity) {
    size_t used = 0;
    char byte;
    while (console.nextOutput(byte)) {
        if (used + 1 < capacity) output[used++] = byte;
    }
    output[used] = '\0';
}

void monitorConsoleTransitionsAndPriority() {
    dev_console::Console console;
    Calls calls;
    char output[2048];
    assert(!console.inConsole());

    console.queueDiagnostic("boot diagnostic\r\n");
    feed(console, "\n", calls);
    assert(console.inConsole());
    drain(console, output, sizeof(output));
    assert(std::strcmp(output, "\r\naac> ") == 0);
    assert(std::strstr(output, "boot diagnostic") == nullptr);

    console.queueDiagnostic("suppressed one\r\n");
    feed(console, "help\r\n", calls);
    drain(console, output, sizeof(output));
    assert(std::strstr(output, "help       Show commands") != nullptr);
    assert(std::strstr(output, "time YYYY-MM-DD HH:MM:SS") != nullptr);
    assert(std::strstr(output, "suppressed one") == nullptr);
    assert(std::strstr(output, "aac> ") != nullptr);

    feed(console, "status\n", calls);
    drain(console, output, sizeof(output));
    assert(calls.status == 1);
    assert(std::strstr(output, "STATUS RESPONSE") != nullptr);

    feed(console, "exit\r\n", calls);
    assert(!console.inConsole());
    drain(console, output, sizeof(output));
    assert(std::strstr(output, "Returning to monitor mode\r\n") != nullptr);
    assert(std::strstr(output, "suppressed one") == nullptr);
    console.queueDiagnostic("monitor resumed\r\n");
    drain(console, output, sizeof(output));
    assert(std::strcmp(output, "monitor resumed\r\n") == 0);
}

void editingWhitespaceUnknownAndOverflow() {
    dev_console::Console console;
    Calls calls;
    char output[2048];
    feed(console, "\r", calls);
    drain(console, output, sizeof(output));

    // CRLF is one submission. Backspace and Delete both edit the line.
    feed(console, "  statusX\b\r\n", calls);
    drain(console, output, sizeof(output));
    assert(calls.status == 1);
    assert(std::strstr(output, "\b \b") != nullptr);
    feed(console, "statusX\x7f\n", calls);
    drain(console, output, sizeof(output));
    assert(calls.status == 2);

    feed(console, "wat\n", calls);
    drain(console, output, sizeof(output));
    assert(std::strstr(output, "Error: unknown command 'wat'") != nullptr);

    char long_line[128];
    std::memset(long_line, 'x', sizeof(long_line) - 2);
    long_line[sizeof(long_line) - 2] = '\n';
    long_line[sizeof(long_line) - 1] = '\0';
    feed(console, long_line, calls);
    drain(console, output, sizeof(output));
    assert(std::strstr(output, "Error: command too long") != nullptr);
    feed(console, "reset\n", calls);
    drain(console, output, sizeof(output));
    assert(calls.reset == 1); // Overflow recovery accepts the next command.
}

void timeValidationAndBoundedDiagnostics() {
    dev_console::Console console;
    Calls calls;
    char output[2048];
    feed(console, "\n", calls);
    drain(console, output, sizeof(output));

    feed(console, "time 2024-02-29 23:59:59\n", calls);
    drain(console, output, sizeof(output));
    assert(calls.set_time == 1);
    assert(calls.epoch == 1709251199LL);

    const char* invalid[] = {
        "time 2023-02-29 12:00:00\n",
        "time 1900-01-01 00:00:00\n",
        "time 2100-01-01 00:00:00\n",
        "time 2024-13-01 00:00:00\n",
        "time 2024-01-01 24:00:00\n",
        "time 2024-01-01 00:60:00\n",
        "time 2024-01-01 00:00:60\n",
        "time 2024/01/01 00:00:00\n",
        "time 2024-01-01 00:00:00 extra\n",
    };
    for (const char* command : invalid) {
        feed(console, command, calls);
        drain(console, output, sizeof(output));
        assert(std::strstr(output, "Usage: time") != nullptr);
    }
    assert(calls.set_time == 1);

    char huge[1100];
    std::memset(huge, 'd', sizeof(huge) - 1);
    huge[sizeof(huge) - 1] = '\0';
    console.queueDiagnostic("retained\r\n");
    console.queueDiagnostic(huge); // Whole record rejected at bounded capacity.
    assert(console.diagnosticBytesDropped() >= std::strlen(huge));
    drain(console, output, sizeof(output));
    assert(std::strstr(output, "retained") == nullptr); // Quiet in console.

    feed(console, "exit\n", calls);
    drain(console, output, sizeof(output));
    assert(std::strstr(output, "retained") == nullptr); // No backlog replay.
}
}

int main() {
    monitorConsoleTransitionsAndPriority();
    editingWhitespaceUnknownAndOverflow();
    timeValidationAndBoundedDiagnostics();
}
