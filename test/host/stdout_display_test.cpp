#include <cassert>
#include <string>
#include "stdout_display.hpp"

struct Sink : display_output::Sink {
    display_output::Availability state = display_output::Availability::ready;
    size_t capacity = 112;
    std::string bytes;
    display_output::Availability availability(size_t required) const override {
        if (state != display_output::Availability::ready) return state;
        return capacity >= required ? state : display_output::Availability::blocked;
    }
    bool write(const uint8_t* data, size_t length) override {
        if (availability(length) != display_output::Availability::ready) return false;
        bytes.append(reinterpret_cast<const char*>(data), length);
        return true;
    }
};

int main() {
    Sink sink;
    stdout_display::Driver driver(sink);
    presentation::State state;
    state.clock.utc_valid = state.clock.pps_locked = true;
    state.clock.utc.year = 2026; state.clock.utc.month = 1; state.clock.utc.day = 1;
    state.clock.utc.hour = 12; state.clock.utc.minute = 34; state.clock.utc.second = 56;
    state.zone = presentation::DisplayZone::eastern;
    driver.begin(state);
    driver.service(state);
    assert(!sink.bytes.empty() && sink.bytes.size() <= 112);
    assert(sink.bytes.find("2026-01-01 12:34:56 UTC") != std::string::npos);
    assert(sink.bytes.find("EST 07:34:56") != std::string::npos);
    assert(sink.bytes.find("LOCKED") != std::string::npos);

    sink.bytes.clear();
    sink.state = display_output::Availability::quiet;
    driver.service(state); // Discard pending output while console owns CDC.
    state.clock.utc.year = 2026; state.clock.utc.month = 1; state.clock.utc.day = 2;
    state.clock.utc.hour = 1; state.clock.utc.minute = 2; state.clock.utc.second = 3;
    sink.state = display_output::Availability::ready;
    driver.service(state);
    assert(sink.bytes.find("2026-01-02 01:02:03 UTC") != std::string::npos);
    assert(sink.bytes.find("2026-01-01") == std::string::npos);

    sink.bytes.clear();
    sink.capacity = 0;
    state.now_us += 1000000;
    driver.service(state); // No USB room: keep only the newest replaceable frame.
    assert(sink.bytes.empty());
    state.now_us += 1000000;
    state.clock.utc.day = 3;
    driver.service(state); // Refresh while blocked; no stale backlog is queued.
    sink.capacity = 112;
    driver.service(state);
    assert(sink.bytes.find("2026-01-03") != std::string::npos);
    assert(sink.bytes.find("2026-01-02") == std::string::npos);

    sink.bytes.clear();
    state.clock.utc_valid = false;
    state.now_us += 1000000;
    driver.service(state);
    assert(sink.bytes.find("UTC INVALID") != std::string::npos);
    assert(sink.bytes.find("LOCKED") == std::string::npos);
}
