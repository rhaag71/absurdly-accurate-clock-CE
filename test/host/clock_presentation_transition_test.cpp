#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "clock_presentation_transition.hpp"
#include "compact_clock_font.hpp"
#include "informational_overlay.hpp"
#include "presentation_time_event.hpp"
#include "ws2812_pixels.hpp"

namespace {
using clock_presentation_transition::State;
using display_appearance::Rgb;
using presentation_time_event::Event;
using ws2812_display::Pixels;

constexpr Rgb black{0, 0, 0};
constexpr Rgb white{255, 240, 220};

int centeredX(const char* text) {
  return (ws2812_display::logical_width - compact_clock_font::textWidth(text)) /
         2;
}

Rgb logicalPixel(const Pixels& pixels, int x, int y) {
  uint16_t raw = 0U;
  assert(ws2812_display::logicalToRaw(x, y, raw));
  return pixels.at(raw);
}

bool same(Rgb left, Rgb right) {
  return left.red == right.red && left.green == right.green &&
         left.blue == right.blue;
}

bool samePixels(const Pixels& left, const Pixels& right) {
  for (std::size_t i = 0; i < ws2812_display::pixel_count; ++i) {
    if (!same(left.at(i), right.at(i))) {
      return false;
    }
  }
  return true;
}

unsigned countColor(const Pixels& pixels, Rgb color) {
  unsigned count = 0U;
  for (std::size_t i = 0; i < ws2812_display::pixel_count; ++i) {
    if (same(pixels.at(i), color)) {
      ++count;
    }
  }
  return count;
}

void advanceDissolveToCompletion(State& state, uint32_t& now_us,
                                 uint8_t hour, const char* text) {
  for (uint8_t bucket = 0; bucket < clock_dissolve::kBucketCount; ++bucket) {
    now_us += clock_dissolve::kBucketIntervalUs;
    assert(state.update(true, hour, 0, text, centeredX(text), now_us).changed);
  }
  now_us += clock_dissolve::kMidpointHoldUs;
  assert(state.update(true, hour, 0, text, centeredX(text), now_us).changed);
  for (uint8_t bucket = 0; bucket < clock_dissolve::kBucketCount; ++bucket) {
    now_us += clock_dissolve::kBucketIntervalUs;
    assert(state.update(true, hour, 0, text, centeredX(text), now_us).changed);
  }
  assert(!state.dissolveActive());
}

void testEventResolver() {
  presentation_time_event::Resolver resolver;
  assert(resolver.update(true, 4, 58) == Event::none);
  assert(resolver.update(true, 4, 59) == Event::none);
  assert(resolver.update(true, 5, 0) == Event::ordinary_hour);

  resolver.reset();
  assert(resolver.update(true, 5, 59) == Event::none);
  assert(resolver.update(true, 6, 0) == Event::six_am);
  resolver.reset();
  resolver.update(true, 11, 59);
  assert(resolver.update(true, 12, 0) == Event::noon);
  resolver.reset();
  resolver.update(true, 17, 59);
  assert(resolver.update(true, 18, 0) == Event::six_pm);
  resolver.reset();
  resolver.update(true, 23, 59);
  assert(resolver.update(true, 0, 0) == Event::midnight);

  resolver.reset();
  assert(resolver.update(true, 5, 0) == Event::none);
  resolver.reset();
  resolver.update(true, 3, 17);
  assert(resolver.update(true, 6, 0) == Event::none);

  // Classification uses the new civil hour and does not require hour+1.
  resolver.reset();
  resolver.update(true, 8, 59);
  assert(resolver.update(true, 7, 0) == Event::ordinary_hour);
  resolver.reset();
  resolver.update(true, 1, 59);
  assert(resolver.update(true, 0, 0) == Event::midnight);

  resolver.update(false, 0, 0);
  assert(resolver.update(true, 12, 0) == Event::none);
}

void testDissolvePrimitive() {
  static_assert(sizeof(clock_dissolve::ClockMask) == 32U,
                "each glyph mask must remain 32 bytes");
  clock_dissolve::ClockMask first;
  clock_dissolve::ClockMask second;
  assert(clock_dissolve::buildMask(first, "9:59", centeredX("9:59")));
  assert(clock_dissolve::buildMask(second, "9:59", centeredX("9:59")));
  assert(first == second);

  bool observed[clock_dissolve::kBucketCount]{};
  for (uint8_t y = 0; y < ws2812_display::logical_height; ++y) {
    for (uint8_t x = 0; x < ws2812_display::logical_width; ++x) {
      const uint8_t bucket = clock_dissolve::bucketFor(x, y);
      assert(bucket < clock_dissolve::kBucketCount);
      observed[bucket] = true;
    }
  }
  for (uint8_t bucket = 0; bucket < clock_dissolve::kBucketCount; ++bucket) {
    assert(observed[bucket]);
  }
  assert(clock_dissolve::bucketFor(0, 0) !=
         clock_dissolve::bucketFor(1, 0));
  assert(clock_dissolve::bucketFor(0, 0) !=
         clock_dissolve::bucketFor(0, 1));

  clock_dissolve::State dissolve;
  assert(dissolve.start("9:59", "10:00", 6, 4, 1000U));
  Pixels frame;
  dissolve.render(frame, black, white);
  const unsigned old_pixel_count = countColor(frame, white);
  assert(old_pixel_count > 0U);
  assert(dissolve.update(1000U + clock_dissolve::kBucketIntervalUs - 1U) ==
         clock_dissolve::Advance::none);
  assert(dissolve.progress() == 0U);
  assert(dissolve.update(1000U + clock_dissolve::kBucketIntervalUs) ==
         clock_dissolve::Advance::changed);
  dissolve.render(frame, black, white);
  assert(countColor(frame, white) < old_pixel_count);

  // A very late call still advances only one bucket.
  assert(dissolve.update(1000000U) == clock_dissolve::Advance::changed);
  assert(dissolve.progress() == 2U);

  uint32_t now = 1000000U;
  while (dissolve.phase() == clock_dissolve::Phase::old_out) {
    now += clock_dissolve::kBucketIntervalUs;
    dissolve.update(now);
  }
  dissolve.render(frame, black, white);
  assert(countColor(frame, white) == 0U);
  assert(dissolve.update(now + clock_dissolve::kMidpointHoldUs - 1U) ==
         clock_dissolve::Advance::none);
  now += clock_dissolve::kMidpointHoldUs;
  assert(dissolve.update(now) == clock_dissolve::Advance::changed);
  dissolve.render(frame, black, white);
  assert(countColor(frame, white) == 0U);
  for (uint8_t bucket = 0; bucket < clock_dissolve::kBucketCount; ++bucket) {
    now += clock_dissolve::kBucketIntervalUs;
    dissolve.update(now);
  }
  assert(!dissolve.active());
  clock_dissolve::ClockMask expected_new;
  assert(clock_dissolve::buildMask(expected_new, "10:00", 6));
  assert(dissolve.newMask() == expected_new);
}

void testOrdinaryHourSequencing() {
  State state;
  Pixels frame;
  uint32_t now = 0U;

  // First valid time appears centered with no startup transition.
  auto result = state.update(true, 4, 58, "4:58", centeredX("4:58"), now);
  assert(result.event == Event::none && !state.dissolveActive());
  assert(state.currentX() == centeredX("4:58"));
  result = state.update(true, 4, 59, "4:59", centeredX("4:59"), ++now);
  assert(result.event == Event::none && !state.dissolveActive());

  // Same-origin ordinary hour dissolves and settles without recentering.
  result = state.update(true, 5, 0, "5:00", centeredX("5:00"), ++now);
  assert(result.event == Event::ordinary_hour && state.dissolveActive());
  assert(!state.recenterActive());
  const uint32_t before_step = now + clock_dissolve::kBucketIntervalUs - 1U;
  assert(!state.update(true, 5, 0, "5:00", centeredX("5:00"), before_step)
              .changed);
  now = before_step + 1U;
  assert(state.update(true, 5, 0, "5:00", centeredX("5:00"), now).changed);
  for (uint8_t remaining = 1; remaining < clock_dissolve::kBucketCount;
       ++remaining) {
    now += clock_dissolve::kBucketIntervalUs;
    state.update(true, 5, 0, "5:00", centeredX("5:00"), now);
  }
  now += clock_dissolve::kMidpointHoldUs;
  state.update(true, 5, 0, "5:00", centeredX("5:00"), now);
  for (uint8_t bucket = 0; bucket < clock_dissolve::kBucketCount; ++bucket) {
    now += clock_dissolve::kBucketIntervalUs;
    state.update(true, 5, 0, "5:00", centeredX("5:00"), now);
  }
  assert(!state.dissolveActive() && !state.recenterActive());
  assert(state.currentX() == centeredX("5:00"));

  // 9:59 -> 10:00 resolves at x=6, then and only then hands off 6 -> 5 -> 4.
  state.reset();
  now = 0U;
  state.update(true, 9, 59, "9:59", centeredX("9:59"), now);
  result = state.update(true, 10, 0, "10:00", centeredX("10:00"), ++now);
  assert(result.event == Event::ordinary_hour && state.dissolveActive());
  assert(state.currentX() == 6 && state.targetX() == 6);
  advanceDissolveToCompletion(state, now, 10, "10:00");
  assert(state.recenterActive() && state.currentX() == 6 && state.targetX() == 4);
  Pixels expected;
  state.render(frame, black, white);
  expected.fill(black);
  assert(compact_clock_font::drawText(expected, "10:00", 6, 0, white));
  assert(samePixels(frame, expected));
  assert(!state.update(true, 10, 0, "10:00", 4,
                       now + clock_recenter_slide::step_interval_us - 1U)
              .changed);
  now += clock_recenter_slide::step_interval_us;
  assert(state.update(true, 10, 0, "10:00", 4, now).changed);
  assert(state.currentX() == 5);
  now += clock_recenter_slide::step_interval_us;
  assert(state.update(true, 10, 0, "10:00", 4, now).changed);
  assert(state.currentX() == 4 && !state.recenterActive());

  // 12:59 -> 1:00 uses the same sequencing in the opposite direction.
  state.reset();
  now = 0U;
  state.update(true, 12, 59, "12:59", centeredX("12:59"), now);
  result = state.update(true, 13, 0, "1:00", centeredX("1:00"), ++now);
  assert(result.event == Event::ordinary_hour && state.dissolveActive());
  assert(state.currentX() == 4);
  advanceDissolveToCompletion(state, now, 13, "1:00");
  assert(state.recenterActive() && state.currentX() == 4 && state.targetX() == 7);
  for (int expected_x = 5; expected_x <= 7; ++expected_x) {
    now += clock_recenter_slide::step_interval_us;
    assert(state.update(true, 13, 0, "1:00", 7, now).changed);
    assert(state.currentX() == expected_x);
  }
  assert(!state.recenterActive());

  // The full primary transition is 8*35 + 40 + 8*35 = 600 ms when serviced
  // at every deadline; no service call performs a blocking catch-up.
  static_assert(2U * clock_dissolve::kBucketCount *
                        clock_dissolve::kBucketIntervalUs +
                    clock_dissolve::kMidpointHoldUs ==
                600000U,
                "initial dissolve tuning must remain 600 ms");
}

void testSpecialsInterruptionAndInvalid() {
  State state;
  uint32_t now = 0U;
  auto result = state.update(true, 5, 59, "5:59", centeredX("5:59"), now);
  result = state.update(true, 6, 0, "6:00", centeredX("6:00"), ++now);
  assert(result.event == Event::six_am);
  assert(!state.dissolveActive() && !state.recenterActive());
  assert(state.currentX() == centeredX("6:00"));

  state.reset();
  state.update(true, 11, 59, "11:59", centeredX("11:59"), 0U);
  result = state.update(true, 12, 0, "12:00", centeredX("12:00"), 1U);
  assert(result.event == Event::noon);
  assert(!state.dissolveActive() && !state.recenterActive());
  assert(state.currentX() == centeredX("12:00"));

  state.reset();
  state.update(true, 9, 59, "9:59", centeredX("9:59"), 0U);
  state.update(true, 10, 0, "10:00", centeredX("10:00"), 1U);
  assert(state.dissolveActive());
  result = state.update(true, 10, 17, "10:17", centeredX("10:17"), 2U);
  assert(result.changed && !state.dissolveActive() && !state.recenterActive());
  assert(std::strcmp(state.text(), "10:17") == 0);
  assert(state.currentX() == centeredX("10:17"));

  state.reset();
  state.update(true, 9, 59, "9:59", centeredX("9:59"), 0U);
  state.update(true, 10, 0, "10:00", centeredX("10:00"), 1U);
  assert(state.dissolveActive());
  result = state.update(false, 0, 0, "", 0, 2U);
  assert(result.changed && !state.hasText() && !state.dissolveActive());
  result = state.update(true, 12, 0, "12:00", centeredX("12:00"), 3U);
  assert(result.event == Event::none && !state.dissolveActive());
  assert(state.currentX() == centeredX("12:00"));
}

void testOverlayIndependence() {
  State state;
  Pixels frame;
  uint32_t now = 0U;
  state.update(true, 9, 59, "9:59", centeredX("9:59"), now);
  state.update(true, 10, 0, "10:00", centeredX("10:00"), ++now);
  now += clock_dissolve::kBucketIntervalUs;
  state.update(true, 10, 0, "10:00", centeredX("10:00"), now);
  state.render(frame, black, white);

  informational_overlay::Frame overlay;
  overlay.has_day = true;
  overlay.day = 15;
  overlay.health_state = informational_overlay::Health::green;
  overlay.holdover = true;
  overlay.holdover_intensity = 8;
  assert(informational_overlay::apply(frame, overlay));
  assert(same(logicalPixel(frame, 14, 7), display_appearance::calendar_blue));
  assert(same(logicalPixel(frame, 31, 4), display_appearance::health_green));
  assert(same(logicalPixel(frame, 31, 3), display_appearance::holdover_red));
}
}  // namespace

int main() {
  testEventResolver();
  testDissolvePrimitive();
  testOrdinaryHourSequencing();
  testSpecialsInterruptionAndInvalid();
  testOverlayIndependence();
}
