#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "clock_presentation_transition.hpp"
#include "compact_clock_font.hpp"
#include "informational_overlay.hpp"
#include "ws2812_pixels.hpp"

namespace {
using clock_presentation_transition::State;
using clock_six_pm_effect::Phase;
using display_appearance::Rgb;
using presentation_time_event::Event;
using ws2812_display::Pixels;

constexpr Rgb kBlack{0, 0, 0};
constexpr Rgb kNormal{255, 240, 220};
constexpr Rgb kOtherNormal{40, 100, 200};

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

void assertMaskColor(const Pixels& pixels,
                     const clock_dissolve::ClockMask& mask,
                     Rgb on_color, Rgb off_color = kBlack) {
  for (uint8_t y = 0; y < ws2812_display::logical_height; ++y) {
    for (uint8_t x = 0; x < ws2812_display::logical_width; ++x) {
      assert(same(logicalPixel(pixels, x, y),
                  mask.contains(x, y) ? on_color : off_color));
    }
  }
}

bool equalsTranslatedMask(const Pixels& pixels,
                         const clock_dissolve::ClockMask& mask,
                         int vertical_offset) {
  for (int y = 0; y < ws2812_display::logical_height; ++y) {
    for (int x = 0; x < ws2812_display::logical_width; ++x) {
      const int source_y = y - vertical_offset;
      const bool expected = source_y >= 0 &&
                            source_y < ws2812_display::logical_height &&
                            mask.contains(static_cast<uint8_t>(x),
                                          static_cast<uint8_t>(source_y));
      const bool actual = !same(logicalPixel(pixels, x, y), kBlack);
      if (actual != expected) return false;
    }
  }
  return true;
}

void testSixPmEntryAndClassification() {
  State transition;
  auto result = transition.update(true, 17, 58, "5:58", centeredX("5:58"), 0U);
  assert(result.event == Event::none && !transition.sixPmActive());
  result = transition.update(true, 17, 59, "5:59", centeredX("5:59"), 1U);
  assert(result.event == Event::none && !transition.sixPmActive());
  result = transition.update(true, 18, 0, "6:00", centeredX("6:00"), 2U);
  assert(result.event == Event::six_pm);
  assert(transition.sixPmActive());
  assert(!transition.sixPmDitherRequested());  // Clear/rain remain steady white.
  assert(!transition.dissolveActive() && !transition.recenterActive());
  assert(centeredX("5:59") == centeredX("6:00"));
  assert(transition.currentX() == centeredX("6:00"));

  State startup;
  result = startup.update(true, 18, 0, "6:00", centeredX("6:00"), 0U);
  assert(result.event == Event::none && !startup.sixPmActive());
  assert(!startup.sixPmDitherRequested());
  assert(!startup.dissolveActive() && !startup.recenterActive());

  State arbitrary_jump;
  arbitrary_jump.update(true, 17, 17, "5:17", centeredX("5:17"), 0U);
  result = arbitrary_jump.update(true, 18, 0, "6:00", centeredX("6:00"), 1U);
  assert(result.event == Event::none && !arbitrary_jump.sixPmActive());
  assert(!arbitrary_jump.sixPmDitherRequested());
  assert(!arbitrary_jump.dissolveActive());

  State first_valid;
  first_valid.update(true, 18, 0, "6:00", centeredX("6:00"), 0U);
  Pixels frame;
  first_valid.render(frame, kBlack, kNormal);
  clock_dissolve::ClockMask expected;
  assert(clock_dissolve::buildMask(expected, "6:00", centeredX("6:00")));
  assertMaskColor(frame, expected, kNormal);
}

void testRainAssemblyAndDeterminism() {
  clock_six_pm_effect::State rain;
  clock_six_pm_effect::State repeat;
  const int final_x = centeredX("6:00");
  assert(rain.start("6:00", final_x, 100U));
  assert(repeat.start("6:00", final_x, 100U));
  clock_dissolve::ClockMask expected;
  assert(clock_dissolve::buildMask(expected, "6:00", final_x));
  assert(rain.finalMask() == expected);
  assert(sizeof(rain) <= 512U);
  assert(rain.leftX() >= 0 && rain.rightX() < ws2812_display::logical_width);

  // There is one compact independent record per occupied final glyph pixel.
  // 6:00 has 51 lit positions with this font; no translated-mask offset or
  // second RGB framebuffer is used for the fall phase.
  assert(rain.elementCount() == 51U);
  assert(rain.elementCount() <= clock_six_pm_effect::kMaximumFallingElements);
  assert(sizeof(clock_six_pm_effect::FallingElement) <= 6U);
  assert(sizeof(clock_six_pm_effect::FallingElement) *
             clock_six_pm_effect::kMaximumFallingElements == 384U);

  bool occupied_column[ws2812_display::logical_width]{};
  uint8_t x_at_rank[clock_six_pm_effect::kMaximumFallingElements]{};
  uint8_t y_at_rank[clock_six_pm_effect::kMaximumFallingElements]{};
  uint32_t release_at_rank[clock_six_pm_effect::kMaximumFallingElements]{};
  uint8_t row_count[ws2812_display::logical_height]{};
  uint32_t row_rank_sum[ws2812_display::logical_height]{};
  for (uint8_t y = 0; y < ws2812_display::logical_height; ++y) {
    for (uint8_t x = 0; x < ws2812_display::logical_width; ++x) {
      if (expected.contains(x, y)) {
        occupied_column[x] = true;
        ++row_count[y];
      }
    }
  }
  assert(expected.contains(static_cast<uint8_t>(rain.leftX()), 3));
  assert(expected.contains(static_cast<uint8_t>(rain.rightX()), 3));

  // One global deterministic order interleaves destinations. The row bias
  // favors lower pixels statistically, but there is no row completion gate.
  for (uint8_t y = 0; y < ws2812_display::logical_height; ++y) {
    for (uint8_t x = 0; x < ws2812_display::logical_width; ++x) {
      if (!expected.contains(x, y)) continue;
      assert(rain.releaseRankAt(x, y) == repeat.releaseRankAt(x, y));
      assert(rain.releaseOffsetAt(x, y) == repeat.releaseOffsetAt(x, y));
      const uint8_t rank = rain.releaseRankAt(x, y);
      assert(rank < rain.elementCount());
      x_at_rank[rank] = x;
      y_at_rank[rank] = y;
      release_at_rank[rank] = rain.releaseOffsetAt(x, y);
      row_rank_sum[y] += rank;
    }
  }
  for (uint8_t rank = 0U; rank < rain.elementCount(); ++rank) {
    assert(release_at_rank[rank] == repeat.releaseOffsetAt(
                                        x_at_rank[rank], y_at_rank[rank]));
    if (rank + 1U < rain.elementCount()) {
      const uint32_t gap_us = release_at_rank[rank + 1U] - release_at_rank[rank];
      assert(gap_us >= clock_six_pm_effect::kMinimumReleaseGapMs * 1000U);
      assert(gap_us <= (clock_six_pm_effect::kMinimumReleaseGapMs +
                        clock_six_pm_effect::kReleaseGapRangeMs - 1U) *
                           1000U);
      assert(gap_us == rain.releaseGapAfterRankUs(rank));
    }
  }
  assert(clock_six_pm_effect::kVerticalStepIntervalUs == 50000U);
  assert(row_rank_sum[6] / row_count[6] < row_rank_sum[0] / row_count[0]);

  bool interleaved_rows = false;
  bool upper_before_lower_complete = false;
  bool columns_develop_unevenly = false;
  for (uint8_t a = 0U; a < rain.elementCount(); ++a) {
    for (uint8_t b = static_cast<uint8_t>(a + 1U); b < rain.elementCount(); ++b) {
      if (y_at_rank[a] != y_at_rank[b]) interleaved_rows = true;
      if (y_at_rank[a] < y_at_rank[b] && a < b)
        upper_before_lower_complete = true;
      if (x_at_rank[a] != x_at_rank[b] &&
          (a > 12U) != (b > 12U)) columns_develop_unevenly = true;
    }
  }
  assert(interleaved_rows);
  assert(upper_before_lower_complete);
  assert(columns_develop_unevenly);
  bool variable_gaps = false;
  for (uint8_t rank = 1U; rank + 1U < rain.elementCount(); ++rank)
    if (release_at_rank[rank + 1U] - release_at_rank[rank] !=
        release_at_rank[rank] - release_at_rank[rank - 1U])
      variable_gaps = true;
  assert(variable_gaps);

  const uint32_t duration = rain.rainDurationUs();
  assert(duration >= 2800000U && duration <= 4000000U);
  assert(duration == 3129000U);  // Fixed 6:00 mask and deterministic schedule.
  bool cadence_checked = false;
  for (uint8_t i = 0U; i < rain.elementCount() && !cadence_checked; ++i) {
    const auto& element = rain.elementAt(i);
    if (element.final_y < 3U) continue;
    const uint32_t release =
        rain.releaseOffsetAt(element.final_x, element.final_y);
    assert(rain.fallingYAt(element.final_x, element.final_y,
                           release + 49000U) == -1);
    assert(rain.fallingYAt(element.final_x, element.final_y,
                           release + 50000U) == 0);
    assert(rain.fallingYAt(element.final_x, element.final_y,
                           release + 100000U) == 1);
    cadence_checked = true;
  }
  assert(cadence_checked);
  assert(rain.update(100U + clock_six_pm_effect::kClearBeatUs - 1U, 18, 0)
             .changed == false);
  assert(rain.update(100U + clock_six_pm_effect::kClearBeatUs, 18, 0).changed);
  assert(rain.phase() == Phase::rain);

  uint8_t element_x[clock_six_pm_effect::kMaximumFallingElements]{};
  int8_t previous_y[clock_six_pm_effect::kMaximumFallingElements];
  for (uint8_t i = 0U; i < clock_six_pm_effect::kMaximumFallingElements; ++i)
    previous_y[i] = -1;
  bool observed_individual_steps = false;
  for (uint8_t i = 0; i < rain.elementCount(); ++i) {
    element_x[i] = rain.elementAt(i).final_x;
    assert(rain.elementAt(i).current_y == -1);
  }

  Pixels frame;
  const uint32_t rain_started = 100U + clock_six_pm_effect::kClearBeatUs;
  uint8_t in_flight_rows = 0U;
  bool saw_mixed_row_states = false;
  bool higher_pixel_fell_while_bottom_incomplete = false;
  bool saw_uneven_vertical_sections = false;
  bool saw_multiple_visible_rows = false;
  for (uint32_t elapsed = 10000U; elapsed < duration; elapsed += 10000U) {
    const uint32_t now = rain_started + elapsed;
    rain.update(now, 18, 0);
    rain.render(frame, kBlack, kNormal);
    bool visible = false;
    for (uint8_t py = 0; py < ws2812_display::logical_height; ++py) {
      for (uint8_t px = 0; px < ws2812_display::logical_width; ++px) {
        if (!same(logicalPixel(frame, px, py), kBlack)) {
          visible = true;
          assert(occupied_column[px]);
        }
      }
    }
    if (elapsed >= 500000U && elapsed <= 2200000U && elapsed % 100000U == 0U) {
      uint8_t active_y_bits = 0U;
      uint8_t moving = 0U;
      bool row_landed[ws2812_display::logical_height]{};
      bool row_falling[ws2812_display::logical_height]{};
      bool row_unreleased[ws2812_display::logical_height]{};
      uint8_t column_landed[ws2812_display::logical_width]{};
      for (uint8_t i = 0; i < rain.elementCount(); ++i) {
        const auto& element = rain.elementAt(i);
        assert(element.final_x == element_x[i]);
        if (element.current_y >= static_cast<int8_t>(element.final_y)) {
          row_landed[element.final_y] = true;
          ++column_landed[element.final_x];
        } else if (element.current_y >= 0) {
          row_falling[element.final_y] = true;
        } else {
          row_unreleased[element.final_y] = true;
        }
        if (element.current_y >= 0 && element.current_y < element.final_y) {
          ++moving;
          active_y_bits |= static_cast<uint8_t>(1U << element.current_y);
        }
      }
      if (moving >= 2U &&
          (active_y_bits & static_cast<uint8_t>(active_y_bits - 1U)) != 0U)
        in_flight_rows = active_y_bits;
      uint8_t rows_with_all_states = 0U;
      for (uint8_t y = 0U; y < ws2812_display::logical_height; ++y)
        if (row_landed[y] && row_falling[y] && row_unreleased[y])
          ++rows_with_all_states;
      saw_mixed_row_states |= rows_with_all_states >= 2U;
      bool bottom_unfinished = false;
      for (uint8_t i = 0U; i < rain.elementCount(); ++i) {
        const auto& e = rain.elementAt(i);
        if (e.final_y == 6U && e.current_y < 6) bottom_unfinished = true;
      }
      for (uint8_t i = 0U; i < rain.elementCount(); ++i) {
        const auto& e = rain.elementAt(i);
        if (e.final_y < 6U && e.current_y >= 0 && e.current_y < e.final_y &&
            bottom_unfinished) higher_pixel_fell_while_bottom_incomplete = true;
      }
      uint8_t min_column = UINT8_MAX;
      uint8_t max_column = 0U;
      uint8_t visible_rows = 0U;
      for (uint8_t y = 0U; y < ws2812_display::logical_height; ++y) {
        bool row_visible = false;
        for (uint8_t x = 0U; x < ws2812_display::logical_width; ++x)
          if (!same(logicalPixel(frame, x, y), kBlack)) row_visible = true;
        if (row_visible) ++visible_rows;
      }
      saw_multiple_visible_rows |= visible_rows >= 3U;
      for (uint8_t x = 0U; x < ws2812_display::logical_width; ++x) {
        if (!occupied_column[x]) continue;
        if (column_landed[x] < min_column) min_column = column_landed[x];
        if (column_landed[x] > max_column) max_column = column_landed[x];
      }
      saw_uneven_vertical_sections |= max_column >= min_column + 2U;
      for (int offset = -7; offset <= 7; ++offset)
        assert(!equalsTranslatedMask(frame, expected, offset));
    }
    for (uint8_t i = 0U; i < rain.elementCount(); ++i) {
      const auto& element = rain.elementAt(i);
      assert(element.final_x == element_x[i]);
      if (element.current_y > previous_y[i]) {
        assert(element.current_y <= previous_y[i] + 1);
        if (element.current_y >= 0) observed_individual_steps = true;
        previous_y[i] = element.current_y;
      }
    }
    if (elapsed >= clock_six_pm_effect::kVerticalStepIntervalUs &&
        elapsed < duration) {
      assert(visible);
    }
  }
  assert(in_flight_rows != 0U);
  assert(saw_mixed_row_states);
  assert(higher_pixel_fell_while_bottom_incomplete);
  assert(saw_uneven_vertical_sections);
  assert(saw_multiple_visible_rows);
  assert(observed_individual_steps);
  assert(rain.update(rain_started + duration, 18, 0).changed);
  assert(rain.phase() == Phase::settle);
  rain.render(frame, kBlack, kNormal);
  assertMaskColor(frame, expected, kNormal);
  for (uint8_t i = 0; i < rain.elementCount(); ++i) {
    const auto& element = rain.elementAt(i);
    assert(element.current_y == static_cast<int8_t>(element.final_y));
  }
}

void enterOutbound(clock_six_pm_effect::State& effect, uint32_t& now) {
  const int left = effect.leftX();
  const int right = effect.rightX();
  assert(effect.update(now + clock_six_pm_effect::kClearBeatUs, 18, 0).changed);
  now += clock_six_pm_effect::kClearBeatUs;
  assert(effect.phase() == Phase::rain);
  now += effect.rainDurationUs();
  assert(effect.update(now, 18, 0).changed);
  assert(effect.phase() == Phase::settle);
  now += clock_six_pm_effect::kSettleUs - 1U;
  assert(!effect.update(now, 18, 0).changed);
  assert(effect.phase() == Phase::settle);
  ++now;
  assert(effect.update(now, 18, 0).changed);
  assert(effect.phase() == Phase::outbound);
  assert(effect.sweepX() == right && right > left);
}

void testSettleAndScanner() {
  clock_six_pm_effect::State effect;
  uint32_t now = 1000U;
  assert(effect.start("6:00", centeredX("6:00"), now));
  assert(clock_six_pm_effect::kSettleUs == 550000U);
  enterOutbound(effect, now);
  const int left = effect.leftX();
  const int right = effect.rightX();
  const auto mask = effect.finalMask();
  Pixels frame;

  // On this top-left-origin mapping, increasing logical x moves visually
  // right. The fast pass therefore starts at visual right and decrements x.
  // Only the exact glyph slice is amber; the rest of each panel column stays
  // untouched.
  effect.render(frame, kBlack, kNormal);
  for (uint8_t y = 0; y < ws2812_display::logical_height; ++y) {
    for (uint8_t x = 0; x < ws2812_display::logical_width; ++x) {
      const Rgb expected = !mask.contains(x, y)
                               ? kBlack
                               : x == right ? clock_six_pm_effect::kEveningAmber
                                            : kNormal;
      assert(same(logicalPixel(frame, x, y), expected));
    }
  }
  uint8_t glyph_pixels_in_right_column = 0U;
  for (uint8_t y = 0; y < ws2812_display::logical_height; ++y)
    if (mask.contains(static_cast<uint8_t>(right), y))
      ++glyph_pixels_in_right_column;
  assert(glyph_pixels_in_right_column < ws2812_display::logical_height);
  assert(clock_six_pm_effect::kOutboundColumnIntervalUs == 40000U);

  // Advance once per 40 ms while visually moving right-to-left. Previously
  // visited slices return to normal, leaving no amber trail.
  while (effect.phase() == Phase::outbound && effect.sweepX() > left) {
    const int prior_x = effect.sweepX();
    now += clock_six_pm_effect::kOutboundColumnIntervalUs - 1U;
    assert(!effect.update(now, 18, 0).changed);
    ++now;
    assert(effect.update(now, 18, 0).changed);
    assert(effect.sweepX() == prior_x - 1);
    effect.render(frame, kBlack, kNormal);
    for (uint8_t y = 0; y < ws2812_display::logical_height; ++y) {
      for (uint8_t x = 0; x < ws2812_display::logical_width; ++x) {
        if (mask.contains(x, y)) {
          assert(same(logicalPixel(frame, x, y),
                      x == effect.sweepX() ? clock_six_pm_effect::kEveningAmber
                                           : kNormal));
        }
      }
    }
  }
  assert(effect.phase() == Phase::right_hold && effect.sweepX() == left);
  assert(clock_six_pm_effect::kRightHoldUs == 350000U);
  effect.render(frame, kBlack, kNormal);
  for (uint8_t y = 0; y < ws2812_display::logical_height; ++y) {
    for (uint8_t x = 0; x < ws2812_display::logical_width; ++x) {
      if (mask.contains(x, y)) {
        assert(same(logicalPixel(frame, x, y),
                    x == left ? clock_six_pm_effect::kEveningAmber : kNormal));
      }
    }
  }
  now += clock_six_pm_effect::kRightHoldUs - 1U;
  assert(!effect.update(now, 18, 0).changed);
  ++now;
  assert(effect.update(now, 18, 0).changed);
  assert(effect.phase() == Phase::returning && effect.sweepX() == left);

  // The slow painting pass begins at the visual left edge and moves right at
  // 80 ms per x. Passed left-side slices stay amber; unvisited right-side
  // slices remain normal.
  assert(clock_six_pm_effect::kReturnColumnIntervalUs == 80000U);
  while (effect.phase() == Phase::returning) {
    const int prior_x = effect.sweepX();
    now += clock_six_pm_effect::kReturnColumnIntervalUs - 1U;
    assert(!effect.update(now, 18, 0).changed);
    ++now;
    assert(effect.update(now, 18, 0).changed);
    if (effect.phase() == Phase::idle) break;
    assert(effect.sweepX() == prior_x + 1);
    effect.render(frame, kBlack, kNormal);
    for (uint8_t y = 0; y < ws2812_display::logical_height; ++y) {
      for (uint8_t x = 0; x < ws2812_display::logical_width; ++x) {
        if (mask.contains(x, y)) {
          const Rgb expected = x <= effect.sweepX()
                                   ? clock_six_pm_effect::kEveningAmber
                                   : kNormal;
          assert(same(logicalPixel(frame, x, y), expected));
        } else {
          assert(same(logicalPixel(frame, x, y), kBlack));
        }
      }
    }
  }
  assert(effect.tintActive() && effect.amberPercent() == 100U);
  assert(effect.sweepX() == right);
}

void completePrimary(State& transition, uint32_t& now) {
  assert(!transition.sixPmDitherRequested());
  while (transition.sixPmEffect().phase() == Phase::clear_beat) {
    now += clock_six_pm_effect::kClearBeatUs;
    transition.update(true, 18, 0, "6:00", centeredX("6:00"), now);
    assert(!transition.sixPmDitherRequested());
  }
  if (transition.sixPmEffect().phase() == Phase::rain) {
    now += transition.sixPmEffect().rainDurationUs();
    transition.update(true, 18, 0, "6:00", centeredX("6:00"), now);
    assert(!transition.sixPmDitherRequested());
  }
  while (transition.sixPmEffect().phase() == Phase::settle) {
    now += clock_six_pm_effect::kSettleUs;
    transition.update(true, 18, 0, "6:00", centeredX("6:00"), now);
    assert(transition.sixPmDitherRequested());
  }
  while (transition.sixPmEffect().phase() == Phase::outbound) {
    now += clock_six_pm_effect::kOutboundColumnIntervalUs;
    transition.update(true, 18, 0, "6:00", centeredX("6:00"), now);
    assert(transition.sixPmDitherRequested());
  }
  if (transition.sixPmEffect().phase() == Phase::right_hold) {
    now += clock_six_pm_effect::kRightHoldUs;
    transition.update(true, 18, 0, "6:00", centeredX("6:00"), now);
    assert(transition.sixPmDitherRequested());
  }
  while (transition.sixPmEffect().phase() == Phase::returning) {
    now += clock_six_pm_effect::kReturnColumnIntervalUs;
    transition.update(true, 18, 0, "6:00", centeredX("6:00"), now);
    assert(transition.sixPmDitherRequested());
  }
  assert(!transition.sixPmActive() && transition.eveningTintActive());
  assert(transition.sixPmDitherRequested());  // Full-amber 18:00 hold.
}

void testTintScheduleAndLiveClock() {
  State direct_1803;
  auto result = direct_1803.update(true, 18, 3, "6:03", centeredX("6:03"),
                                   0U);
  assert(result.event == Event::none);
  assert(!direct_1803.sixPmActive() && !direct_1803.eveningTintActive());
  assert(!direct_1803.sixPmDitherRequested());
  assert(direct_1803.eveningAmberPercent() == 0U);

  State transition;
  uint32_t now = 0U;
  transition.update(true, 17, 59, "5:59", centeredX("5:59"), now);
  result = transition.update(true, 18, 0, "6:00", centeredX("6:00"), ++now);
  assert(result.event == Event::six_pm);
  completePrimary(transition, now);
  Pixels complete_amber;
  transition.render(complete_amber, kBlack, kOtherNormal);
  clock_dissolve::ClockMask complete_mask;
  assert(clock_dissolve::buildMask(complete_mask, "6:00", transition.currentX()));
  assertMaskColor(complete_amber, complete_mask,
                  clock_six_pm_effect::kEveningAmber);

  const uint8_t expected_percent[] = {100U, 80U, 60U, 40U, 20U, 0U};
  for (uint8_t minute = 0; minute <= 5U; ++minute) {
    if (minute != 0U) {
      now += 100U;
      char text[6] = {'6', ':', static_cast<char>('0' + minute / 10U),
                      static_cast<char>('0' + minute % 10U), '\0', '\0'};
      result = transition.update(true, 18, minute, text, centeredX(text), now);
      assert(result.changed);
    }
    assert(transition.eveningAmberPercent() == expected_percent[minute]);
    assert(transition.sixPmDitherRequested() == (minute == 0U));
    Pixels frame;
    transition.render(frame, kBlack, kOtherNormal);
    clock_dissolve::ClockMask expected_mask;
    char text[6] = {'6', ':', static_cast<char>('0' + minute / 10U),
                    static_cast<char>('0' + minute % 10U), '\0', '\0'};
    assert(clock_dissolve::buildMask(expected_mask, text, transition.currentX()));
    const Rgb blended = clock_six_pm_effect::State::blendColor(
        kOtherNormal, clock_six_pm_effect::kEveningAmber,
        expected_percent[minute]);
    assertMaskColor(frame, expected_mask, blended);
  }
  assert(!transition.eveningTintActive());
  assert(!transition.sixPmDitherRequested());

  // Tint remains presentation paint over the current base color and does not
  // overwrite the configured clock color used by later renders.
  const Rgb full_amber = clock_six_pm_effect::State::blendColor(
      kOtherNormal, clock_six_pm_effect::kEveningAmber, 100U);
  assert(same(full_amber, clock_six_pm_effect::kEveningAmber));
  const Rgb eighty_percent = clock_six_pm_effect::State::blendColor(
      kOtherNormal, clock_six_pm_effect::kEveningAmber, 80U);
  assert(same(eighty_percent, {212, 97, 40}));
  assert(same(kOtherNormal, {40, 100, 200}));

  State discontinuity;
  discontinuity.update(true, 17, 59, "5:59", centeredX("5:59"), 0U);
  discontinuity.update(true, 18, 0, "6:00", centeredX("6:00"), 1U);
  uint32_t changed_at = 1U;
  completePrimary(discontinuity, changed_at);
  result = discontinuity.update(true, 18, 3, "6:03", centeredX("6:03"),
                                ++changed_at);
  assert(result.changed && !discontinuity.eveningTintActive());
  assert(discontinuity.eveningAmberPercent() == 0U);
}

void testInterruptionsInvalidAndOverlays() {
  const Phase interrupt_phases[] = {Phase::rain, Phase::settle,
                                    Phase::outbound, Phase::right_hold,
                                    Phase::returning};
  for (Phase wanted : interrupt_phases) {
    State transition;
    transition.update(true, 17, 59, "5:59", centeredX("5:59"), 0U);
    transition.update(true, 18, 0, "6:00", centeredX("6:00"), 1U);
    uint32_t now = 1U;
    while (transition.sixPmEffect().phase() != wanted) {
      now += wanted == Phase::rain
                 ? clock_six_pm_effect::kClearBeatUs
                 : wanted == Phase::settle
                       ? transition.sixPmEffect().rainDurationUs()
                       : wanted == Phase::outbound
                             ? clock_six_pm_effect::kSettleUs
                             : wanted == Phase::right_hold
                                   ? clock_six_pm_effect::kOutboundColumnIntervalUs
                                   : clock_six_pm_effect::kRightHoldUs;
      transition.update(true, 18, 0, "6:00", centeredX("6:00"), now);
      if (transition.sixPmEffect().phase() == Phase::outbound &&
          wanted == Phase::right_hold) {
        while (transition.sixPmEffect().phase() == Phase::outbound) {
          now += clock_six_pm_effect::kOutboundColumnIntervalUs;
          transition.update(true, 18, 0, "6:00", centeredX("6:00"), now);
        }
      }
      if (transition.sixPmEffect().phase() == wanted) break;
    }
    assert(transition.sixPmEffect().phase() == wanted);
    char replacement[6] = "6:01";
    auto result = transition.update(true, 18, 1, replacement,
                                    centeredX(replacement), ++now);
    assert(result.changed && !transition.sixPmActive());
    assert(!transition.eveningTintActive());
    assert(!transition.sixPmDitherRequested());
    assert(std::strcmp(transition.text(), replacement) == 0);
    assert(transition.currentX() == centeredX(replacement));
    Pixels frame;
    transition.render(frame, kBlack, kNormal);
    clock_dissolve::ClockMask normal_mask;
    assert(clock_dissolve::buildMask(normal_mask, replacement,
                                     centeredX(replacement)));
    assertMaskColor(frame, normal_mask, kNormal);
  }

  State invalid;
  invalid.update(true, 17, 59, "5:59", centeredX("5:59"), 0U);
  invalid.update(true, 18, 0, "6:00", centeredX("6:00"), 1U);
  auto result = invalid.update(false, 0, 0, "", 0, 2U);
  assert(result.changed && !invalid.sixPmActive() && !invalid.eveningTintActive());
  assert(!invalid.sixPmDitherRequested());
  Pixels blank;
  invalid.render(blank, kBlack, kNormal);
  assertMaskColor(blank, clock_dissolve::ClockMask{}, kBlack);

  // The primary paints only time glyphs; persistent pixels are independently
  // applied afterward and remain visible during the dark/rain presentation.
  State overlays;
  overlays.update(true, 17, 59, "5:59", centeredX("5:59"), 0U);
  overlays.update(true, 18, 0, "6:00", centeredX("6:00"), 1U);
  overlays.render(blank, kBlack, kNormal);
  informational_overlay::Frame overlay;
  overlay.has_day = true;
  overlay.day = 15;
  overlay.health_state = informational_overlay::Health::green;
  overlay.holdover = true;
  overlay.holdover_intensity = 8;
  assert(informational_overlay::apply(blank, overlay));
  assert(same(logicalPixel(blank, 14, 7), display_appearance::calendar_blue));
  assert(same(logicalPixel(blank, 31, 4), display_appearance::health_green));
  assert(same(logicalPixel(blank, 31, 3), display_appearance::holdover_red));
}

void testOtherSpecialsUnchanged() {
  const uint8_t hours[] = {6U, 12U, 0U};
  const char* texts[] = {"6:00", "12:00", "12:00"};
  const uint8_t prior_hours[] = {5U, 11U, 23U};
  for (uint8_t i = 0; i < 3U; ++i) {
    State transition;
    char before[6] = {};
    if (prior_hours[i] == 5U) std::strcpy(before, "5:59");
    else if (prior_hours[i] == 11U) std::strcpy(before, "11:59");
    else std::strcpy(before, "11:59");
    transition.update(true, prior_hours[i], 59, before, centeredX(before), 0U);
    const auto result = transition.update(true, hours[i], 0, texts[i],
                                          centeredX(texts[i]), 1U);
    assert(result.event != Event::six_pm);
    assert(!transition.sixPmActive() && !transition.eveningTintActive());
    assert(!transition.dissolveActive() && !transition.recenterActive());
  }
}
}  // namespace

int main() {
  testSixPmEntryAndClassification();
  testRainAssemblyAndDeterminism();
  testSettleAndScanner();
  testTintScheduleAndLiveClock();
  testInterruptionsInvalidAndOverlays();
  testOtherSpecialsUnchanged();
}
