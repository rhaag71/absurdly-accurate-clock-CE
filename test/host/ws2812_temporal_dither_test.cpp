#include <cassert>
#include <cstdint>
#include <cstring>

#include "dev_console.hpp"
#include "display_transfer.hpp"
#include "ws2812_temporal_dither.hpp"
#include "ws2812_wire_frame.hpp"

namespace {
using namespace ws2812_display;

struct ConsoleCapture {
  unsigned handled = 0U;
};

void handle(void*, dev_console::Command command, int64_t value,
            dev_console::Console& console) {
  if (command == dev_console::Command::temporal_dither_set)
    setTemporalDitherMode(static_cast<TemporalDitherMode>(value));
  if (command == dev_console::Command::temporal_dither_status ||
      command == dev_console::Command::temporal_dither_set) {
    console.writeLine(temporalDitherModeName(temporalDitherMode()));
  }
}

void feed(dev_console::Console& console, const char* text) {
  while (*text) console.receive(*text++, handle, nullptr);
}

void drain(dev_console::Console& console, char* output, std::size_t capacity) {
  std::size_t used = 0U;
  char byte = 0;
  while (console.nextOutput(byte)) {
    if (used + 1U < capacity) output[used++] = byte;
  }
  output[used] = '\0';
}

void testFractionalAccuracyAndChannelIndependence() {
  TemporalDither dither;
  uint32_t sums[3] = {};
  for (unsigned frame = 0U; frame < 255U; ++frame) {
    const Rgb scaled = dither.scale({64U, 128U, 200U}, 2U, 0U);
    sums[0] += scaled.red;
    sums[1] += scaled.green;
    sums[2] += scaled.blue;
  }
  assert(sums[0] == 128U);  // 64 * 2 / 255, averaged over 255 frames.
  assert(sums[1] == 256U);  // Independent 1 + 1/255 channel.
  assert(sums[2] == 400U);  // Independent 1 + 145/255 channel.
  assert(sizeof(dither) == pixel_count * 6U + 2U);
}

void testChangesZeroAndFullScale() {
  TemporalDither dither;
  assert(dither.scale({64U, 0U, 0U}, 2U, 0U).red == 0U);
  assert(dither.scale({64U, 0U, 0U}, 2U, 0U).red == 1U);
  // A changed logical input discards the old fractional remainder.
  assert(dither.scale({65U, 0U, 0U}, 2U, 0U).red == 0U);
  // Brightness changes reset carry even if the logical input is unchanged.
  assert(dither.scale({64U, 0U, 0U}, 3U, 0U).red == 0U);
  assert(dither.scale({64U, 0U, 0U}, 0U, 0U).red == 0U);
  assert(dither.scale({64U, 0U, 0U}, 2U, 0U).red == 0U);
  const Rgb full = dither.scale({64U, 128U, 255U}, 255U, 0U);
  assert(full.red == 64U && full.green == 128U && full.blue == 255U);
  const Rgb black = dither.scale({255U, 255U, 255U}, 0U, 1U);
  assert(black.red == 0U && black.green == 0U && black.blue == 0U);
}

void testPerceptualDeadbandAndPhaseSpread() {
  TemporalDither perceptual;
  uint8_t output[32] = {};
  for (unsigned frame = 0U; frame < 20000U; ++frame)
    output[frame % 32U] = perceptual.scale({128U, 0U, 0U}, 2U,
        frame % 32U, TemporalDitherMode::perceptual).red;
  // 128*2/255 is only 1 + 1/255. Perceptual mode intentionally drops that
  // tiny fraction instead of producing a rare bright pulse every ~2.55 s.
  for (uint8_t value : output) assert(value == 1U);

  TemporalDither first;
  TemporalDither repeat;
  bool saw_low = false;
  bool saw_high = false;
  for (std::size_t pixel = 0U; pixel < 32U; ++pixel) {
    const Rgb a = first.scale({64U, 64U, 64U}, 2U, pixel,
        TemporalDitherMode::perceptual);
    const Rgb b = repeat.scale({64U, 64U, 64U}, 2U, pixel,
        TemporalDitherMode::perceptual);
    assert(a.red == b.red && a.green == b.green && a.blue == b.blue);
    saw_low = saw_low || a.red == 0U || b.green == 0U || b.blue == 0U;
    saw_high = saw_high || a.red == 1U || b.green == 1U || b.blue == 1U;
  }
  assert(saw_low && saw_high);  // Deterministic per-pixel phase decorrelation.

  TemporalDither average;
  uint32_t sum = 0U;
  for (unsigned frame = 0U; frame < 255U; ++frame)
    sum += average.scale({64U, 0U, 0U}, 2U, 7U,
        TemporalDitherMode::perceptual).red;
  assert(sum >= 127U && sum <= 129U);  // Target sum is 128.

  TemporalDither exact;
  for (unsigned frame = 0U; frame < 1000U; ++frame)
    assert(exact.scale({255U, 0U, 0U}, 2U, 0U,
        TemporalDitherMode::perceptual).red == 2U);
}

void testModeSwitchResetsWithoutStaleCarry() {
  setTemporalDitherMode(TemporalDitherMode::off);
  TemporalDitherRefreshTracker tracker;
  TemporalDither switched;
  TemporalDither fresh;

  setTemporalDitherMode(TemporalDitherMode::basic);
  assert(tracker.consumeChange());
  switched.reset();
  for (unsigned i = 0U; i < 17U; ++i)
    (void)switched.scale({64U, 0U, 0U}, 2U, 3U,
                         TemporalDitherMode::basic);

  setTemporalDitherMode(TemporalDitherMode::perceptual);
  assert(tracker.consumeChange());
  switched.reset();  // Same generation handoff used by the display service.
  for (unsigned i = 0U; i < 255U; ++i) {
    const Rgb a = switched.scale({64U, 0U, 0U}, 2U, 3U,
                                 TemporalDitherMode::perceptual);
    const Rgb b = fresh.scale({64U, 0U, 0U}, 2U, 3U,
                              TemporalDitherMode::perceptual);
    assert(a.red == b.red);  // No basic-mode residual leaks into perceptual.
  }
  setTemporalDitherMode(TemporalDitherMode::off);
  assert(tracker.consumeChange());
}

void testEffectRequestTemporarilyOverridesUserMode() {
  assert(resolveTemporalDitherMode(false, TemporalDitherMode::off) ==
         TemporalDitherMode::off);
  assert(resolveTemporalDitherMode(true, TemporalDitherMode::off) ==
         TemporalDitherMode::perceptual);
  // Console-selected A/B mode is retained, not mutated, under the effect;
  // it resumes after the effect request ends.
  assert(resolveTemporalDitherMode(true, TemporalDitherMode::basic) ==
         TemporalDitherMode::perceptual);
  assert(resolveTemporalDitherMode(false, TemporalDitherMode::basic) ==
         TemporalDitherMode::basic);
}

void testStaticFrameRefreshAndImmutableDmaBuffer() {
  Pixels pixels;
  assert(pixels.set(0U, {64U, 128U, 200U}));
  const Rgb original = pixels.at(0U);
  TemporalDither dither;
  WireFrame wire;
  bool saw_multiple_words = false;
  uint32_t previous_word = UINT32_MAX;

  for (unsigned frame = 0U; frame < 255U; ++frame) {
    assert(wire.prepareDithered(pixels, 2U,
        display_appearance::TransferMode::off, dither));
    const uint32_t word = wire.words()[0];
    if (frame != 0U && word != previous_word) saw_multiple_words = true;
    previous_word = word;
    assert(wire.start());
    assert(!wire.prepareDithered(pixels, 2U,
        display_appearance::TransferMode::off, dither));
    assert(wire.words()[0] == word);  // The DMA-owned word is immutable.
    assert(!wire.prepare(pixels, 2U, display_appearance::TransferMode::off));
    assert(!wire.poll(frame * 10000U, true));
    const uint32_t drain_at = frame * 10000U + 100U;
    assert(!wire.poll(drain_at, false));
    assert(!wire.poll(drain_at + wire_tail_us + wire_reset_us - 1U, false));
    assert(wire.poll(drain_at + wire_tail_us + wire_reset_us, false));
  }
  assert(saw_multiple_words);  // Static logical input changes over time on wire.
  assert(pixels.at(0U).red == original.red &&
         pixels.at(0U).green == original.green &&
         pixels.at(0U).blue == original.blue);

  TemporalDither expected_dither;
  const Rgb scaled = expected_dither.scale(original, 2U, 0U);
  TemporalDither gamma_dither;
  WireFrame gamma_wire;
  assert(gamma_wire.prepareDithered(pixels, 2U,
      display_appearance::TransferMode::g2, gamma_dither));
  assert(gamma_wire.words()[0] == pixels.grbWordForColor(
      scaled, 255U, display_appearance::TransferMode::g2));

  TemporalDither overlay_dither;
  WireFrame overlay_wire;
  assert(overlay_wire.prepareDithered(pixels, 2U,
      display_appearance::TransferMode::off, overlay_dither));
  uint16_t holdover_raw = 0U;
  assert(logicalToRaw(31, 3, holdover_raw));
  assert(overlay_wire.paintOutputPixel(31, 3, {5U, 0U, 0U}));
  assert(overlay_wire.words()[holdover_raw] == (5U << 16));
}

void testRefreshSchedule() {
  TemporalDitherRefreshSchedule schedule;
  schedule.reset(100U);
  assert(!schedule.due(100U + temporal_dither_period_us - 1U));
  assert(schedule.due(100U + temporal_dither_period_us));
  assert(!schedule.due(100U + temporal_dither_period_us));
  assert(schedule.due(100U + 8U * temporal_dither_period_us));
  assert(!schedule.due(100U + 8U * temporal_dither_period_us));
  assert(temporal_dither_target_hz == 100U);
  schedule.stop();
  assert(!schedule.due(1000000U));
}

void testConsoleToggleAndStatus() {
  setTemporalDitherEnabled(false);
  TemporalDitherRefreshTracker tracker;
  assert(!tracker.consumeChange());
  dev_console::Console console;
  char output[256];
  feed(console, "\n");
  drain(console, output, sizeof(output));
  feed(console, "dither\ndither basic\ndither\ndither perceptual\ndither\ndither off\ndither nope\n");
  drain(console, output, sizeof(output));
  assert(std::strstr(output, "off") != nullptr);
  assert(std::strstr(output, "basic") != nullptr);
  assert(std::strstr(output, "perceptual") != nullptr);
  assert(std::strstr(output, "Usage: dither {off|basic|perceptual}") != nullptr);
  assert(!temporalDitherEnabled());
  assert(tracker.consumeChange());
  assert(!tracker.consumeChange());
}
}  // namespace

int main() {
  testFractionalAccuracyAndChannelIndependence();
  testChangesZeroAndFullScale();
  testPerceptualDeadbandAndPhaseSpread();
  testModeSwitchResetsWithoutStaleCarry();
  testEffectRequestTemporarilyOverridesUserMode();
  testStaticFrameRefreshAndImmutableDmaBuffer();
  testRefreshSchedule();
  testConsoleToggleAndStatus();
}
