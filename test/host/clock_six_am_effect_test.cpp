#include <cassert>
#include <cstdint>

#include "clock_presentation_transition.hpp"
#include "compact_clock_font.hpp"
#include "ws2812_pixels.hpp"

namespace {
using clock_presentation_transition::State;
using clock_six_am_effect::Phase;
using display_appearance::Rgb;
using presentation_time_event::Event;
using ws2812_display::Pixels;

constexpr Rgb kBlack{0U, 0U, 0U};
constexpr Rgb kNormal{255U, 240U, 220U};

int centeredX(const char* text) {
  return (ws2812_display::logical_width - compact_clock_font::textWidth(text)) /
         2;
}

Rgb pixelAt(const Pixels& pixels, uint8_t x, uint8_t y) {
  uint16_t raw = 0U;
  assert(ws2812_display::logicalToRaw(x, y, raw));
  return pixels.at(raw);
}

bool same(Rgb a, Rgb b) {
  return a.red == b.red && a.green == b.green && a.blue == b.blue;
}

void assertMaskColor(const Pixels& pixels,
                     const clock_dissolve::ClockMask& mask, Rgb color) {
  for (uint8_t y = 0U; y < ws2812_display::logical_height; ++y) {
    for (uint8_t x = 0U; x < ws2812_display::logical_width; ++x) {
      assert(same(pixelAt(pixels, x, y), mask.contains(x, y) ? color : kBlack));
    }
  }
}

void assertGlyphPixelsColor(const Pixels& pixels,
                            const clock_dissolve::ClockMask& mask, Rgb color) {
  for (uint8_t y = 0U; y < ws2812_display::logical_height; ++y)
    for (uint8_t x = 0U; x < ws2812_display::logical_width; ++x)
      if (mask.contains(x, y)) assert(same(pixelAt(pixels, x, y), color));
}

void advanceToSunrise(clock_six_am_effect::State& effect, uint32_t start) {
  assert(effect.update(start + clock_six_am_effect::kDarkBeatUs - 1U, 6, 0)
             .changed == false);
  assert(effect.update(start + clock_six_am_effect::kDarkBeatUs, 6, 0).changed);
  assert(effect.phase() == Phase::sunrise);
}

void testSunGeometryRiseAndNegativeSpace() {
  clock_six_am_effect::State effect;
  const int x = centeredX("6:00");
  assert(effect.start("6:00", x, 100U));
  assert(effect.sunCenterX() == x + (compact_clock_font::textWidth("6:00") - 1) / 2);
  assert(effect.sunCenterX() == 15);
  assert(clock_six_am_effect::kDarkBeatUs == 200000U);
  assert(clock_six_am_effect::kSunRiseDurationUs == 3500000U);
  assert(clock_six_am_effect::kBlueWashStartUs ==
         clock_six_am_effect::kSunRiseDurationUs +
             clock_six_am_effect::kSunTopHoldUs);
  assert(clock_six_am_effect::kSunTopHoldUs == 500000U);
  assert(clock_six_am_effect::kBlueWashDurationUs == 2500000U);
  assert(clock_six_am_effect::kSunDissolveBucketCount == 8U);
  assert(clock_six_am_effect::kSunDissolveBucketIntervalUs == 75000U);
  assert(sizeof(effect) < 96U);  // One 32-byte mask; no RGB framebuffer.
  assert(clock_six_am_effect::kSunDiameter == 25U);
  assert(same(clock_six_am_effect::kSunYellow, {255U, 64U, 0U}));
  assert(same(clock_six_am_effect::kMorningBlue, {64U, 160U, 255U}));

  uint16_t circle_pixels = 0U;
  for (int dy = -clock_six_am_effect::kSunRadius;
       dy <= clock_six_am_effect::kSunRadius; ++dy) {
    for (int dx = -clock_six_am_effect::kSunRadius;
         dx <= clock_six_am_effect::kSunRadius; ++dx) {
      if (clock_six_am_effect::State::sunCircleContains(dx, dy))
        ++circle_pixels;
    }
  }
  assert(circle_pixels > 400U);
  assert(clock_six_am_effect::State::sunCircleContains(12, 0));
  assert(!clock_six_am_effect::State::sunCircleContains(13, 0));

  Pixels frame;
  effect.render(frame, kBlack);
  assert(effect.phase() == Phase::dark_beat);
  for (uint16_t i = 0U; i < ws2812_display::pixel_count; ++i)
    assert(same(frame.at(i), kBlack));

  advanceToSunrise(effect, 100U);
  assert(effect.sunCenterYNow() == clock_six_am_effect::kSunStartCenterY);
  effect.render(frame, kBlack);
  uint8_t visible_at_start = 0U;
  for (uint8_t y = 0U; y < ws2812_display::logical_height; ++y)
    for (uint8_t px = 0U; px < ws2812_display::logical_width; ++px) {
      const Rgb color = pixelAt(frame, px, y);
      if (!same(color, kBlack)) {
        ++visible_at_start;
        assert(y == 7U);
      }
    }
  assert(visible_at_start == 0U);  // The complete sun begins below view.

  assert(effect.update(100U + clock_six_am_effect::kDarkBeatUs + 1000000U,
                       6, 0).changed);
  assert(effect.sunCenterYNow() == 16);
  effect.render(frame, kBlack);
  uint8_t warm_rows = 0U;
  bool shadow_pixel = false;
  for (uint8_t y = 0U; y < ws2812_display::logical_height; ++y) {
    bool row_has_sun = false;
    for (uint8_t px = 0U; px < ws2812_display::logical_width; ++px) {
      const Rgb color = pixelAt(frame, px, y);
      if (same(color, clock_six_am_effect::kSunYellow)) {
        row_has_sun = true;
      }
      const int dx = static_cast<int>(px) - effect.sunCenterX();
      const int dy = static_cast<int>(y) - effect.sunCenterYNow();
      if (effect.glyphMaskContains(px, y) &&
          clock_six_am_effect::State::sunCircleContains(dx, dy)) {
        assert(same(color, kBlack));
        shadow_pixel = true;
      }
    }
    if (row_has_sun) ++warm_rows;
  }
  assert(warm_rows >= 3U);  // Not a flat horizon/band.
  assert(shadow_pixel);     // Glyph pixels cut opaque black holes in the sun.

  const uint32_t rise_complete = clock_six_am_effect::kSunRiseDurationUs;
  assert(effect.update(100U + clock_six_am_effect::kDarkBeatUs + rise_complete,
                       6, 0).changed);
  assert(effect.sunCenterYNow() == clock_six_am_effect::kSunEndCenterY);
  effect.render(frame, kBlack);
  for (uint8_t y = 0U; y < ws2812_display::logical_height; ++y)
    for (uint8_t px = 0U; px < ws2812_display::logical_width; ++px)
      if (effect.glyphMaskContains(px, y)) assert(same(pixelAt(frame, px, y), kBlack));

  const uint32_t before_wash = clock_six_am_effect::kBlueWashStartUs - 1U;
  assert(effect.update(100U + clock_six_am_effect::kDarkBeatUs + before_wash,
                       6, 0).changed);
  assert(effect.sunCenterYNow() == clock_six_am_effect::kSunEndCenterY);
  assert(effect.blueFrontAt(0U) == -2);
  effect.render(frame, kBlack);
  for (uint16_t i = 0U; i < ws2812_display::pixel_count; ++i)
    assert(!same(frame.at(i), clock_six_am_effect::kMorningBlue));
}

void testWashAndSunDissolve() {
  clock_six_am_effect::State effect;
  assert(effect.start("6:00", centeredX("6:00"), 0U));
  advanceToSunrise(effect, 0U);
  Pixels frame;

  const uint32_t near_top = clock_six_am_effect::kBlueWashStartUs;
  assert(effect.update(clock_six_am_effect::kDarkBeatUs + near_top, 6, 0)
             .changed);
  assert(near_top == clock_six_am_effect::kSunRiseDurationUs +
                         clock_six_am_effect::kSunTopHoldUs);
  assert(effect.sunCenterYNow() == clock_six_am_effect::kSunEndCenterY);
  assert(effect.sunCenterYNow() - clock_six_am_effect::kSunRadius <= 0);
  int min_front = ws2812_display::logical_height;
  int max_front = -2;
  int8_t front_at_start[ws2812_display::logical_width]{};
  for (uint8_t x = 0U; x < ws2812_display::logical_width; ++x) {
    const int front = effect.blueFrontAt(x);
    front_at_start[x] = static_cast<int8_t>(front);
    if (front < min_front) min_front = front;
    if (front > max_front) max_front = front;
  }
  assert(max_front > min_front);  // Stable per-column hash makes a jagged front.
  assert(effect.primaryDurationUs() ==
         clock_six_am_effect::kBlueWashStartUs +
             clock_six_am_effect::kBlueWashDurationUs +
             clock_six_am_effect::kSunDissolveDurationUs);

  const uint32_t wash_middle = near_top + clock_six_am_effect::kBlueWashDurationUs / 2U;
  assert(effect.update(clock_six_am_effect::kDarkBeatUs + wash_middle, 6, 0)
             .changed);
  for (uint8_t x = 0U; x < ws2812_display::logical_width; ++x)
    assert(effect.blueFrontAt(x) >= front_at_start[x]);
  effect.render(frame, kBlack);
  bool blue_glyph_seen = false;
  bool warm_sun_seen = false;
  for (uint8_t y = 0U; y < ws2812_display::logical_height; ++y) {
    for (uint8_t x = 0U; x < ws2812_display::logical_width; ++x) {
      const Rgb color = pixelAt(frame, x, y);
      if (same(color, clock_six_am_effect::kMorningBlue)) {
        assert(effect.glyphMaskContains(x, y));
        blue_glyph_seen = true;
      }
      if (same(color, clock_six_am_effect::kSunYellow))
        warm_sun_seen = true;
    }
  }
  assert(blue_glyph_seen && warm_sun_seen);  // Intentional overlap.

  const uint32_t wash_end = near_top + clock_six_am_effect::kBlueWashDurationUs;
  assert(effect.update(clock_six_am_effect::kDarkBeatUs + wash_end, 6, 0)
             .changed);
  effect.render(frame, kBlack);
  assertGlyphPixelsColor(frame, effect.glyphMask(),
                         clock_six_am_effect::kMorningBlue);

  uint16_t previous_warm = 0U;
  for (uint16_t i = 0U; i < ws2812_display::pixel_count; ++i) {
    const Rgb color = frame.at(i);
    if (same(color, clock_six_am_effect::kSunYellow))
      ++previous_warm;
  }
  for (uint8_t bucket = 1U; bucket < clock_six_am_effect::kSunDissolveBucketCount;
       ++bucket) {
    const uint32_t elapsed = wash_end +
        static_cast<uint32_t>(bucket) *
            clock_six_am_effect::kSunDissolveBucketIntervalUs;
    assert(effect.update(clock_six_am_effect::kDarkBeatUs + elapsed, 6, 0)
               .changed);
    effect.render(frame, kBlack);
    uint16_t warm_count = 0U;
    for (uint16_t i = 0U; i < ws2812_display::pixel_count; ++i) {
      const Rgb color = frame.at(i);
      if (same(color, clock_six_am_effect::kSunYellow))
        ++warm_count;
    }
    assert(warm_count <= previous_warm);
    previous_warm = warm_count;
    assertGlyphPixelsColor(frame, effect.glyphMask(),
                           clock_six_am_effect::kMorningBlue);
  }
  assert(effect.update(clock_six_am_effect::kDarkBeatUs + effect.primaryDurationUs(),
                       6, 0).completed);
  assert(!effect.primaryActive() && effect.tintActive());
  effect.render(frame, kBlack);
  for (uint16_t i = 0U; i < ws2812_display::pixel_count; ++i)
    assert(same(frame.at(i), kBlack));
}

void completeSixAm(State& state, uint32_t& now) {
  now += clock_six_am_effect::kDarkBeatUs;
  state.update(true, 6, 0, "6:00", centeredX("6:00"), now);
  now += clock_six_am_effect::kSunRiseDurationUs;
  state.update(true, 6, 0, "6:00", centeredX("6:00"), now);
  now += clock_six_am_effect::kBlueWashStartUs +
         clock_six_am_effect::kBlueWashDurationUs +
         clock_six_am_effect::kSunDissolveDurationUs -
         clock_six_am_effect::kSunRiseDurationUs;
  state.update(true, 6, 0, "6:00", centeredX("6:00"), now);
  assert(!state.sixAmActive() && state.morningTintActive());
}

void testEventEntryAndBlueTint() {
  State transition;
  auto result = transition.update(true, 5, 58, "5:58", centeredX("5:58"), 0U);
  assert(result.event == Event::none);
  transition.update(true, 5, 59, "5:59", centeredX("5:59"), 1U);
  result = transition.update(true, 6, 0, "6:00", centeredX("6:00"), 2U);
  assert(result.event == Event::six_am);
  assert(transition.sixAmActive());
  assert(!transition.sixAmDitherRequested());  // Dark beat is still dark.
  assert(!transition.sixPmActive() && !transition.dissolveActive());
  assert(!transition.recenterActive());
  assert(transition.currentX() == centeredX("6:00"));
  assert(transition.sixAmEffect().sunCenterX() ==
         centeredX("6:00") + (compact_clock_font::textWidth("6:00") - 1) / 2);

  const uint32_t sunrise_start = 2U + clock_six_am_effect::kDarkBeatUs;
  transition.update(true, 6, 0, "6:00", centeredX("6:00"), sunrise_start);
  assert(transition.sixAmDitherRequested());
  transition.update(true, 6, 0, "6:00", centeredX("6:00"),
      sunrise_start + clock_six_am_effect::kBlueWashStartUs - 1U);
  assert(transition.sixAmDitherRequested());
  transition.update(true, 6, 0, "6:00", centeredX("6:00"),
      sunrise_start + clock_six_am_effect::kBlueWashStartUs);
  assert(transition.sixAmActive());  // Blue wash is still part of primary.
  assert(!transition.sixAmDitherRequested());

  State startup;
  startup.update(true, 6, 0, "6:00", centeredX("6:00"), 0U);
  assert(!startup.sixAmActive() && !startup.morningTintActive());
  assert(!startup.sixAmDitherRequested());
  Pixels frame;
  startup.render(frame, kBlack, kNormal);
  clock_dissolve::ClockMask expected;
  assert(clock_dissolve::buildMask(expected, "6:00", centeredX("6:00")));
  assertMaskColor(frame, expected, kNormal);

  State direct_0603;
  direct_0603.update(true, 6, 3, "6:03", centeredX("6:03"), 0U);
  assert(!direct_0603.sixAmActive() && !direct_0603.morningTintActive());
  assert(!direct_0603.sixAmDitherRequested());
  direct_0603.render(frame, kBlack, kNormal);
  assert(clock_dissolve::buildMask(expected, "6:03", centeredX("6:03")));
  assertMaskColor(frame, expected, kNormal);

  uint32_t now = 0U;
  State completed;
  completed.update(true, 5, 59, "5:59", centeredX("5:59"), now);
  result = completed.update(true, 6, 0, "6:00", centeredX("6:00"), ++now);
  assert(result.event == Event::six_am && completed.sixAmActive());
  completeSixAm(completed, now);
  assert(completed.morningBluePercent() == 100U);
  assert(!completed.sixAmDitherRequested());
  completed.render(frame, kBlack, kNormal);
  assert(clock_dissolve::buildMask(expected, "6:00", centeredX("6:00")));
  assertMaskColor(frame, expected, clock_six_am_effect::kMorningBlue);

  const uint8_t minutes[] = {1U, 2U, 3U, 4U};
  const uint8_t percents[] = {80U, 60U, 40U, 20U};
  const char* texts[] = {"6:01", "6:02", "6:03", "6:04"};
  for (uint8_t i = 0U; i < 4U; ++i) {
    ++now;
    result = completed.update(true, 6, minutes[i], texts[i],
                              centeredX(texts[i]), now);
    assert(result.changed && !completed.sixAmActive());
    assert(completed.morningBluePercent() == percents[i]);
    assert(!completed.sixAmDitherRequested());
  }
  ++now;
  completed.update(true, 6, 5, "6:05", centeredX("6:05"), now);
  assert(!completed.morningTintActive() && completed.morningBluePercent() == 0U);
  assert(!completed.sixAmDitherRequested());
  completed.render(frame, kBlack, kNormal);
  assert(clock_dissolve::buildMask(expected, "6:05", centeredX("6:05")));
  assertMaskColor(frame, expected, kNormal);

  State aborted;
  aborted.update(true, 5, 59, "5:59", centeredX("5:59"), 0U);
  aborted.update(true, 6, 0, "6:00", centeredX("6:00"), 1U);
  aborted.update(false, 6, 0, "", 0, 2U);
  assert(!aborted.sixAmActive() && !aborted.morningTintActive());
  assert(!aborted.sixAmDitherRequested());

  State ordinary;
  ordinary.update(true, 6, 3, "6:03", centeredX("6:03"), 0U);
  assert(!ordinary.sixAmDitherRequested());
}

}  // namespace

int main() {
  testSunGeometryRiseAndNegativeSpace();
  testWashAndSunDissolve();
  testEventEntryAndBlueTint();
}
