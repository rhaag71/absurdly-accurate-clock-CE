#include <cassert>
#include <cstring>
#include <initializer_list>

#include "clock_text_renderer.hpp"
#include "dev_console.hpp"
#include "informational_overlay.hpp"
#include "ws2812_pixels.hpp"

namespace {
using ws2812_display::Pixels;
using display_appearance::Rgb;

Rgb logicalPixel(const Pixels& pixels, int x, int y) {
    uint16_t raw = 0;
    assert(ws2812_display::logicalToRaw(x, y, raw));
    return pixels.at(raw);
}

bool same(Rgb a, Rgb b) {
    return a.red == b.red && a.green == b.green && a.blue == b.blue;
}

presentation::State healthyState() {
    presentation::State state;
    state.clock.utc_valid = true;
    state.clock.gps_valid = true;
    state.clock.pps_present = true;
    state.clock.pps_locked = true;
    state.clock.satellites.valid = true;
    state.clock.satellites.used = 5;
    state.clock.utc.year = 2026;
    state.clock.utc.month = 3;
    state.clock.utc.day = 1;
    state.clock.utc.hour = 2;
    state.zone = presentation::DisplayZone::eastern;
    return state;
}

struct ConsoleCapture {
    unsigned health = 0;
    unsigned holdover = 0;
    unsigned level = 0;
    unsigned day = 0;
    informational_overlay::HealthProof last_health =
        informational_overlay::HealthProof::live;
};

void capture(void* opaque, dev_console::Command command, int64_t value,
             dev_console::Console&) {
    auto& result = *static_cast<ConsoleCapture*>(opaque);
    if (command == dev_console::Command::status_proof_set) {
        ++result.health;
        result.last_health = static_cast<informational_overlay::HealthProof>(value);
        informational_overlay::setHealthProof(
            result.last_health);
    } else if (command == dev_console::Command::holdover_proof_set) {
        ++result.holdover;
        informational_overlay::setHoldoverProof(value
            ? informational_overlay::HoldoverProof::on
            : informational_overlay::HoldoverProof::live);
    } else if (command == dev_console::Command::level_proof_set) {
        ++result.level;
        informational_overlay::setLevelProof(static_cast<int>(value));
    } else if (command == dev_console::Command::day_proof_set) {
        ++result.day;
        informational_overlay::setDayProof(static_cast<uint8_t>(value));
    }
}

void feed(dev_console::Console& console, const char* text,
          ConsoleCapture& capture_state) {
    while (*text) console.receive(*text++, capture, &capture_state);
}
}

int main() {
    using namespace informational_overlay;
    assert(same(display_appearance::calendar_blue, {0, 0, 255}));
    assert(same(display_appearance::health_green, {0, 255, 32}));
    assert(same(display_appearance::health_amber, {255, 128, 0}));
    assert(same(display_appearance::health_red, {160, 32, 40}));
    assert(same(display_appearance::holdover_red, {255, 0, 0}));
    auto state = healthyState();
    const auto original_utc = state.clock.utc;
    const int64_t original_seconds = state.clock.utc_seconds;
    char formatted_before[6] = {}, formatted_after[6] = {};

    // Live health requires every part of the current qualification chain and
    // fresh satellite data. Any simultaneous failure deterministically wins.
    assert(health(state) == Health::green);
    state.clock.satellites.used = 4;
    assert(health(state) == Health::amber);
    state.clock.satellites.used = 3;
    assert(health(state) == Health::red);
    state.clock.satellites.used = 5;
    state.clock.gps_valid = false;
    assert(health(state) == Health::red);
    state.clock.gps_valid = true;
    state.clock.pps_locked = false;
    assert(health(state) == Health::red);
    state.clock.pps_locked = true;
    state.clock.satellites.valid = false;
    assert(health(state) == Health::red);
    state.clock.satellites.valid = true;

    // Eastern civil time for 2026-03-01 02:00 UTC is February 28. The day
    // marker follows that local date, not the UTC calendar day.
    auto frame = makeFrame(state, false, 0);
    assert(frame.has_day && frame.day == 28);
    state.zone = presentation::DisplayZone::utc;
    frame = makeFrame(state, false, 0);
    assert(frame.day == 1);
    assert(clock_text_renderer::formatHHMM(state, formatted_before));

    ProofOverrides proof;
    proof.health = HealthProof::good;
    assert(health(state, proof) == Health::green);
    assert(same(healthColor(health(state, proof)), display_appearance::health_green));
    proof.health = HealthProof::minimum;
    assert(health(state, proof) == Health::amber);
    assert(same(healthColor(health(state, proof)), display_appearance::health_amber));
    proof.health = HealthProof::bad;
    assert(health(state, proof) == Health::red);
    assert(same(healthColor(health(state, proof)), display_appearance::health_red));
    proof.health = HealthProof::live;
    Pixels pixels;
    pixels.fill({0, 0, 0});
    proof.day = 1;
    frame = makeFrame(state, false, 0, proof);
    assert(apply(pixels, frame));
    assert(same(logicalPixel(pixels, 0, 7), display_appearance::calendar_blue));
    unsigned markers = 0;
    for (int x = 0; x < 32; ++x)
        if (same(logicalPixel(pixels, x, 7), display_appearance::calendar_blue)) ++markers;
    assert(markers == 1);
    assert(!same(logicalPixel(pixels, 31, 7), display_appearance::calendar_blue));

    proof.day = 15;
    pixels.clear();
    frame = makeFrame(state, false, 0, proof);
    assert(apply(pixels, frame));
    assert(same(logicalPixel(pixels, 14, 7), display_appearance::calendar_blue));
    proof.day = 31;
    pixels.clear();
    frame = makeFrame(state, false, 0, proof);
    assert(apply(pixels, frame));
    assert(same(logicalPixel(pixels, 30, 7), display_appearance::calendar_blue));
    assert(!same(logicalPixel(pixels, 31, 7), display_appearance::calendar_blue));

    // The two status positions are independent. Inactive holdover explicitly
    // paints black; active holdover uses the named red family.
    proof.day = 0;
    pixels.fill(display_appearance::soft_white);
    frame = makeFrame(state, false, 0, proof);
    assert(apply(pixels, frame));
    assert(same(logicalPixel(pixels, 31, 4), display_appearance::health_green));
    assert(same(logicalPixel(pixels, 31, 3), {0, 0, 0}));
    proof.health = HealthProof::off;
    frame = makeFrame(state, false, 0, proof);
    assert(apply(pixels, frame));
    assert(same(logicalPixel(pixels, 31, 4), {0, 0, 0}));
    proof.health = HealthProof::live;
    assert(!holdoverActive(false, proof));
    frame = makeFrame(state, true, holdover_max_intensity, proof);
    assert(apply(pixels, frame));
    assert(same(logicalPixel(pixels, 31, 3), holdoverColor()));
    for (uint16_t intensity : {uint16_t{0}, uint16_t{2}, uint16_t{5},
                               uint16_t{8}, uint16_t{10}}) {
        Pixels phase_pixels;
        const auto phase_frame = makeFrame(state, true, intensity, proof);
        assert(apply(phase_pixels, phase_frame));
        assert(same(logicalPixel(phase_pixels, 31, 3),
                    display_appearance::holdover_red));
        assert(phase_frame.holdover_intensity == intensity);
    }
    // Both normal and inverted proof backgrounds are covered by the later
    // persistent overlay layer, without changing the clock glyph mask.
    constexpr char text[] = "8:05";
    pixels.clear();
    assert(display_compositor::composeClock(pixels, text, 32, 0,
        display_appearance::Rgb{0, 0, 0},
        display_compositor::solid(display_appearance::soft_white)));
    Pixels glyph_only = pixels;
    proof.day = 15;
    frame = makeFrame(state, false, 0, proof);
    assert(apply(pixels, frame));
    assert(same(logicalPixel(pixels, 31, 4), display_appearance::health_green));
    assert(same(logicalPixel(pixels, 14, 7), display_appearance::calendar_blue));
    for (int y = 0; y < 7; ++y) {
        for (int x = 0; x < 32; ++x) {
            if (x == 31 && (y == 3 || y == 4)) continue;
            assert(same(logicalPixel(pixels, x, y), logicalPixel(glyph_only, x, y)));
        }
    }
    pixels.clear();
    assert(display_compositor::composeClock(pixels, text, 32, 0,
        display_appearance::soft_white,
        display_compositor::solid({0, 0, 0})));
    assert(apply(pixels, frame));
    assert(same(logicalPixel(pixels, 31, 4), display_appearance::health_green));
    assert(same(logicalPixel(pixels, 14, 7), display_appearance::calendar_blue));

    // Development proofs only alter presentation inputs and return cleanly to
    // live state; authoritative UTC fields remain untouched.
    ConsoleCapture commands;
    dev_console::Console console;
    feed(console, "\n", commands);
    feed(console, "s off\n", commands);
    assert(commands.last_health == HealthProof::off);
    assert(proofOverrides().health == HealthProof::off);
    frame = makeFrame(state, false, 0, proofOverrides());
    assert(frame.health_state == Health::off);
    assert(same(healthColor(frame.health_state), {0, 0, 0}));
    feed(console, "s good\ns min\ns bad\ns live\nh on\nh off\n"
                  "day 1\nday 15\nday 31\nday live\n", commands);
    assert(commands.health == 5 && commands.holdover == 2 && commands.day == 4);
    feed(console, "day 32\ns unknown\nh maybe\n", commands);
    assert(commands.health == 5 && commands.holdover == 2 && commands.day == 4);
    assert(proofOverrides().health == HealthProof::live);
    assert(proofOverrides().holdover == HoldoverProof::live);
    assert(proofOverrides().day == 0);
    feed(console, "lvl 1\nlvl 2\nlvl 4\nlvl 8\nlvl 12\nlvl 16\nlvl 24\nlvl 32\n", commands);
    assert(commands.level == 8 && proofOverrides().level == 32);
    feed(console, "lvl 33\nlvl -1\nlvl 99999999999999999999\nlvl 1 extra\n", commands);
    assert(commands.level == 8 && proofOverrides().level == 32);
    feed(console, "lvl off\n", commands);
    assert(commands.level == 9 && proofOverrides().level == -1);
    feed(console, "lvl 0\nh on\n", commands);
    assert(proofOverrides().level == -1 && holdoverActive(false, proofOverrides()));
    feed(console, "lvl 8\nh off\n", commands);
    assert(proofOverrides().level == -1 && !holdoverActive(false, proofOverrides()));
    assert(clock_text_renderer::formatHHMM(state, formatted_after));
    assert(std::strcmp(formatted_before, formatted_after) == 0);
    assert(state.clock.utc.year == original_utc.year &&
           state.clock.utc.month == original_utc.month &&
           state.clock.utc.day == original_utc.day &&
           state.clock.utc_seconds == original_seconds);
}
