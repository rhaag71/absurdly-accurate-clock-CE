#include <cassert>
#include <cstdio>
#include <set>
#include "informational_overlay.hpp"
#include "ws2812_wire_frame.hpp"

using namespace informational_overlay;
using namespace ws2812_display;

int main() {
    // Candidate peaks are comparison ranges, not physically approved levels.
    for (uint8_t peak : {8, 12, 16, 24, 32}) {
        std::set<uint8_t> codes;
        unsigned repeats = 0, jump = 0;
        uint8_t previous = 1;
        for (uint32_t t = 0; t <= holdover_period_us / 2;
             t += holdover_redraw_interval_us) {
            const double envelope = ws2812_intensity::cosineEnvelope(t, holdover_period_us);
            const auto code = ws2812_intensity::channelCode(envelope, 1, peak);
            assert(code >= previous && code <= peak);
            if (t && code == previous) ++repeats;
            const unsigned difference = code - previous;
            if (difference > jump) jump = difference;
            codes.insert(code);
            previous = code;
        }
        assert(codes.size() == peak && previous == peak && jump == 1);
        std::printf("Direct range 1..%u: %zu real codes, %u repeated adjacent samples, "
                    "maximum jump %u\n", peak, codes.size(), repeats, jump);
    }
    assert(holdover_cutoff == 0.5);
    assert(holdover_min_intensity == 1 && holdover_max_intensity == 8);
    BreathingAnimator animator;
    animator.reset(0);
    std::set<uint8_t> codes;
    std::set<uint8_t> nonzero_codes;
    uint8_t previous = 0;
    unsigned repeats = 0, jump = 0;
    uint32_t first_nonzero_us = 0, last_minimum_code_us = 0;
    for (uint32_t t = 0; t <= holdover_period_us / 2;
         t += holdover_redraw_interval_us) {
        const auto current = animator.update(t);
        assert(current >= previous && current <= holdover_max_intensity);
        assert(animator.update(t) == current); // Stable; no dither state.
        if (t && current == previous) ++repeats;
        const unsigned difference = current - previous;
        if (difference > jump) jump = difference;
        if (current && !first_nonzero_us) first_nonzero_us = t;
        if (current == holdover_min_intensity) last_minimum_code_us = t;
        if (current) nonzero_codes.insert(current);
        codes.insert(current);
        previous = current;
    }
    assert(previous == holdover_max_intensity);
    assert(codes.size() == 9 && nonzero_codes.size() == 8);
    assert(repeats == 242 && jump == 1 && first_nonzero_us == 1260000);
    assert(last_minimum_code_us == 1460000);
    std::printf("Holdover cutoff %.1f: %zu nonzero levels (%u..%u), "
                "%u/250 repeated samples, max jump %u; OFF dwell ~2.5 s, "
                "lowest-code plateau ~0.21 s per side\n",
                holdover_cutoff, nonzero_codes.size(), holdover_min_intensity,
                holdover_max_intensity, repeats, jump);
    assert(animator.update(holdover_period_us / 2) == holdover_max_intensity);

    // 0.5 is deliberately OFF; the first envelope sample above it enters at
    // useful level 1. The mirrored falling edge returns to OFF symmetrically.
    animator.reset(0);
    assert(animator.update(1240000) == 0);
    assert(animator.update(1250000) == 0);
    assert(animator.update(1260000) == 1);
    assert(animator.update(1460000) == 1);
    assert(animator.update(1470000) == 2);
    assert(animator.update(3530000) == 2);
    assert(animator.update(3540000) == 1);
    assert(animator.update(3740000) == 1);
    assert(animator.update(3750000) == 0);
    assert(animator.update(3760000) == 0);
    for (uint32_t t = 0; t <= holdover_period_us / 2; t += 10000)
        assert(animator.level() <= holdover_max_intensity);
    for (uint32_t t = 0; t <= holdover_period_us; t += 10000)
        assert(ws2812_intensity::cutoffChannelCode(
            ws2812_intensity::cosineEnvelope(t, holdover_period_us),
            holdover_cutoff, holdover_min_intensity,
            holdover_max_intensity) ==
            ws2812_intensity::cutoffChannelCode(
            ws2812_intensity::cosineEnvelope(holdover_period_us - t,
                holdover_period_us), holdover_cutoff,
            holdover_min_intensity, holdover_max_intensity));
    assert(animator.update(holdover_period_us + 1) == 0);

    // Different call frequencies and missed frames yield the same phase/code.
    BreathingAnimator sparse, frequent;
    sparse.reset(0); frequent.reset(0);
    for (uint32_t t = 137; t < 17000000; t += 137) {
        frequent.update(t);
        if (t % (137 * 41) == 0) {
            assert(sparse.update(t) == frequent.level());
            assert(sparse.envelope() == frequent.envelope());
        }
    }
    assert(ws2812_intensity::channelCode(-1,1,16) == 1);
    assert(ws2812_intensity::channelCode(2,1,16) == 16);
    assert(ws2812_intensity::channelCode(0.5,1,16) == 5);
    assert(ws2812_intensity::cutoffChannelCode(0.5,0.5,1,8) == 0);
    assert(ws2812_intensity::cutoffChannelCode(0.500001,0.5,1,8) == 1);
    assert(ws2812_intensity::cutoffChannelCode(1.0,0.5,1,8) == 8);
    uint8_t last_code = 0;
    for (unsigned n = 0; n <= 10000; ++n) {
        const double x = n / 10000.0;
        const auto code = ws2812_intensity::channelCode(x,0,255);
        assert(code == ws2812_intensity::channelCode(x,0,255));
        assert(code >= last_code);
        last_code = code;
    }

    assert(last_code == 255);

    // Sustained deterministic generation through micros wrap: 100,000
    // frames / 200 breath cycles. For every frame, consume the frozen stream
    // in eight-word chunks while attempting forbidden refreshes/settings
    // changes. Delayed completion and latch wrap must never restart a frame.
    Pixels logical;
    WireFrame wire;
    assert(!wire.start());
    assert(!wire.paintOutputPixel(0,0,{0,0,0}));
    BreathingAnimator a, b;
    const uint32_t start = 0xffff0000u;
    a.reset(start);
    b.reset(start);
    uint16_t raw_holdover;
    assert(logicalToRaw(31,3,raw_holdover));
    uint32_t frozen[pixel_count];
    for (unsigned n = 0; n < 100000; ++n) {
        const uint32_t now = start + n * holdover_redraw_interval_us;
        const auto target = a.update(now);
        assert(target == b.update(now));
        Frame overlay;
        overlay.holdover = true;
        overlay.holdover_intensity = target;
        const auto rgb = outputRed(overlay);
        assert(rgb.red == target && rgb.green == 0 && rgb.blue == 0);
        assert(rgb.red <= 8);
        assert(outputRed(overlay).red == rgb.red);
        assert(display_compositor::composeClock(logical,
            n % 1500 < 750 ? "8:05" : "12:34", 32, 0,
            display_appearance::Rgb{0,0,0},
            display_compositor::solid(display_appearance::soft_white)));
        logical.setLogical(14,7,display_appearance::calendar_blue);
        logical.setLogical(31,4,display_appearance::health_green);
        const auto transfer = n % 997 == 0 ? display_appearance::TransferMode::ws
                                          : display_appearance::TransferMode::off;
        assert(wire.prepare(logical,2,transfer));
        assert(wire.paintOutputPixel(31,3,rgb));
        assert(!wire.paintOutputPixel(32,3,rgb));
        for (unsigned i = 0; i < pixel_count; ++i) {
            frozen[i] = wire.words()[i];
            if (i != raw_holdover) assert(frozen[i] == logical.grbWord(i,2,transfer));
        }
        assert(wire.start());
        assert(!wire.start());
        assert(!wire.prepare(logical,255,display_appearance::TransferMode::g2));
        assert(!wire.paintOutputPixel(0,0,{255,0,255}));
        assert(!wire.poll(now,true));
        for (unsigned chunk = 0; chunk < pixel_count; chunk += 8)
            for (unsigned i = chunk; i < chunk + 8; ++i)
                assert(wire.words()[i] == frozen[i]);
        assert(!wire.poll(now,false)); // Queued words still need to drain.
        assert(!wire.poll(now + wire_tail_us + wire_reset_us - 1,false));
        assert(!wire.prepare(logical,255,transfer));
        assert(wire.poll(now + wire_tail_us + wire_reset_us,false));
    }
    // Isolated characterization clears every prior pixel and uses a stable
    // direct code, with no clock, health, or transfer leakage.
    Frame proof;
    for (int level : {0,1,2,4,8,12,16,24,32}) {
        proof.level_proof = level;
        logical.setLogical(0,0,{255,255,255});
        isolateLevelProof(logical,proof);
        assert(wire.prepare(logical,255,display_appearance::TransferMode::g2));
        assert(wire.paintOutputPixel(31,3,outputRed(proof)));
        for (unsigned i = 0; i < pixel_count; ++i)
            assert(wire.words()[i] == (i == raw_holdover ? uint32_t(level) << 16 : 0));
    }
    std::printf("Stable direct holdover: 9 total codes (OFF plus red 1..8); "
                "100000 immutable frames / 200 cycles passed\n");
}
