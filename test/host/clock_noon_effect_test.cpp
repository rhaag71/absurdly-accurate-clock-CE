#include <cassert>
#include <cstdint>

#include "clock_dissolve.hpp"
#include "clock_noon_effect.hpp"
#include "compact_clock_font.hpp"
#include "display_appearance.hpp"
#include "ws2812_pixels.hpp"
#include "ws2812_temporal_dither.hpp"

namespace {
using clock_noon_effect::Phase;
using clock_noon_effect::State;
using display_appearance::Rgb;
using ws2812_display::Pixels;

constexpr Rgb black{0, 0, 0};
constexpr Rgb warm_white{255, 255, 220};

int centeredX(const char* text) {
  return (ws2812_display::logical_width - compact_clock_font::textWidth(text)) /
         2;
}

bool on(const Pixels& frame, int x, int y, Rgb color = warm_white) {
  uint16_t raw = 0U;
  assert(ws2812_display::logicalToRaw(x, y, raw));
  const Rgb pixel = frame.at(raw);
  return pixel.red == color.red && pixel.green == color.green &&
         pixel.blue == color.blue;
}

bool lit(const Pixels& frame, int x, int y) {
  uint16_t raw = 0U;
  assert(ws2812_display::logicalToRaw(x, y, raw));
  const Rgb pixel = frame.at(raw);
  return pixel.red != 0U || pixel.green != 0U || pixel.blue != 0U;
}

bool matchesMask(const Pixels& frame, const clock_dissolve::ClockMask& mask) {
  for (uint8_t y = 0U; y < ws2812_display::logical_height; ++y) {
    for (uint8_t x = 0U; x < ws2812_display::logical_width; ++x) {
      if (on(frame, x, y) != mask.contains(x, y)) return false;
    }
  }
  return true;
}

bool equalsTranslated(const Pixels& frame, const clock_dissolve::ClockMask& mask) {
  for (int dy = -7; dy <= 7; ++dy) {
    for (int dx = -31; dx <= 31; ++dx) {
      bool equal = true;
      for (uint8_t y = 0U; y < ws2812_display::logical_height && equal; ++y) {
        for (uint8_t x = 0U; x < ws2812_display::logical_width; ++x) {
          const int source_x = static_cast<int>(x) - dx;
          const int source_y = static_cast<int>(y) - dy;
          const bool expected = source_x >= 0 &&
                                source_x < ws2812_display::logical_width &&
                                source_y >= 0 &&
                                source_y < ws2812_display::logical_height &&
                                mask.contains(static_cast<uint8_t>(source_x),
                                              static_cast<uint8_t>(source_y));
          if (lit(frame, x, y) != expected) {
            equal = false;
            break;
          }
        }
      }
      if (equal) return true;
    }
  }
  return false;
}

void testIndependentDeterministicParticlesAndSequence() {
  static_assert(sizeof(clock_noon_effect::Particle) == 7U,
                "one compact independent trajectory per particle");
  static_assert(sizeof(State) == 492U,
                "Noon state remains a bounded 492-byte object");
  static_assert(ws2812_display::effect_temporal_dither_rate_hz == 110U,
                "production effect rendering target remains 110 Hz");
  assert(clock_noon_effect::kExplosionUs + clock_noon_effect::kMidpointUs +
             clock_noon_effect::kReassemblyUs + clock_noon_effect::kSettleUs ==
         1920000U);

  const int old_x = centeredX("11:59");
  const int new_x = centeredX("12:00");
  assert(old_x != new_x);
  State first;
  State second;
  assert(first.start("11:59", old_x, "12:00", new_x, 1000U));
  assert(second.start("11:59", old_x, "12:00", new_x, 1000U));
  assert(first.phase() == Phase::explosion);
  assert(first.ditherRequested());
  assert(first.particleCount() <= clock_noon_effect::kMaximumParticles);
  assert(first.finalMask() == second.finalMask());

  Pixels source_frame;
  first.render(source_frame, black, warm_white);
  clock_dissolve::ClockMask old_mask;
  assert(clock_dissolve::buildMask(old_mask, "11:59", old_x));
  assert(matchesMask(source_frame, old_mask));

  bool saw_variation = false;
  uint8_t hot_colors = 0U;
  bool exited_left = false;
  bool exited_right = false;
  bool exited_top = false;
  bool exited_bottom = false;
  uint8_t center_directions = 0U;
  uint8_t palette_counts[4]{};
  int left = ws2812_display::logical_width;
  int right = -1;
  int top = ws2812_display::logical_height;
  int bottom = -1;
  for (uint8_t y = 0U; y < ws2812_display::logical_height; ++y) {
    for (uint8_t x = 0U; x < ws2812_display::logical_width; ++x) {
      if (!old_mask.contains(x, y)) continue;
      if (x < left) left = x;
      if (x > right) right = x;
      if (y < top) top = y;
      if (y > bottom) bottom = y;
    }
  }
  for (uint8_t i = 0U; i < first.particleCount(); ++i) {
    const clock_noon_effect::Particle& a = first.particleAt(i);
    const clock_noon_effect::Particle& b = second.particleAt(i);
    assert(a.source_x == b.source_x && a.source_y == b.source_y &&
           a.burst_x == b.burst_x && a.burst_y == b.burst_y &&
           a.final_x == b.final_x && a.final_y == b.final_y &&
           a.variation == b.variation);
    assert(old_mask.contains(static_cast<uint8_t>(a.source_x),
                             static_cast<uint8_t>(a.source_y)));
    assert(first.finalMask().contains(a.final_x, a.final_y));
    const Rgb palette = first.hotColor(i);
    uint8_t palette_index = 0U;
    if (palette.green == 96U && palette.blue == 0U) {
      palette_index = 1U;
    } else if (palette.green == 0U && palette.blue == 255U) {
      palette_index = 2U;
    } else if (palette.green == 160U && palette.blue == 0U) {
      palette_index = 3U;
    } else {
      assert(palette.green == 0U && palette.blue == 0U);
    }
    ++palette_counts[palette_index];
    if (palette_index == 3U) ++hot_colors;
    assert(first.hotColor(i).red == second.hotColor(i).red &&
           first.hotColor(i).green == second.hotColor(i).green &&
           first.hotColor(i).blue == second.hotColor(i).blue);

    const int rel_x = 2 * static_cast<int>(a.source_x) - left - right;
    const int rel_y = 2 * static_cast<int>(a.source_y) - top - bottom;
    if (rel_x < -4) assert(a.burst_x < a.source_x);
    if (rel_x > 4) assert(a.burst_x > a.source_x);
    if (rel_y < -4) assert(a.burst_y < a.source_y);
    if (rel_y > 4) assert(a.burst_y > a.source_y);
    exited_left = exited_left || a.burst_x < 0;
    exited_right = exited_right || a.burst_x >= ws2812_display::logical_width;
    exited_top = exited_top || a.burst_y < 0;
    exited_bottom = exited_bottom ||
                    a.burst_y >= ws2812_display::logical_height;
    if (rel_x >= -4 && rel_x <= 4 && rel_y >= -4 && rel_y <= 4) {
      const int dx = static_cast<int>(a.burst_x) - a.source_x;
      const int dy = static_cast<int>(a.burst_y) - a.source_y;
      center_directions |= dx == 0 ? 1U : (dy == 0 ? 2U : 4U);
    }
    if (i > 0U && a.variation != first.particleAt(0U).variation) {
      saw_variation = true;
    }
  }
  assert(saw_variation);
  assert(hot_colors > 0U);
  for (uint8_t count : palette_counts) assert(count >= 6U && count <= 20U);
  assert(exited_left && exited_right && exited_top && exited_bottom);
  assert((center_directions & 0x07U) == 0x07U);
  assert(first.explosionColor(0U, 0U, warm_white).red == warm_white.red &&
         first.explosionColor(0U, 0U, warm_white).green == warm_white.green &&
         first.explosionColor(0U, 0U, warm_white).blue == warm_white.blue);
  const clock_noon_effect::Particle& first_particle = first.particleAt(0U);
  const uint32_t first_release_ms =
      first_particle.variation %
      (clock_noon_effect::kExplosionReleaseRangeMs + 1U);
  const Rgb attack_mid = first.explosionColor(
      0U, first_release_ms * 1000U +
              clock_noon_effect::kExplosionColorAttackUs / 2U,
      warm_white);
  assert(attack_mid.red == 255U && attack_mid.green > 0U);
  const uint32_t saturated_outbound_us =
      static_cast<uint32_t>(clock_noon_effect::kExplosionReleaseRangeMs) * 1000U +
      clock_noon_effect::kExplosionColorAttackUs;
  for (uint8_t i = 0U; i < first.particleCount(); ++i) {
    const Rgb outbound = first.explosionColor(i, saturated_outbound_us, warm_white);
    const Rgb assigned = first.hotColor(i);
    assert(outbound.red == assigned.red && outbound.green == assigned.green &&
           outbound.blue == assigned.blue);
  }
  const Rgb first_hot = first.hotColor(0U);
  const Rgb outbound_hot = first.explosionColor(
      0U, clock_noon_effect::kExplosionUs, warm_white);
  assert(outbound_hot.red == first_hot.red && outbound_hot.green == first_hot.green &&
         outbound_hot.blue == first_hot.blue);

  // At a representative instant some source pixels remain unreleased and
  // others are travelling independently; no single translated mask explains
  // the mixed frame.
  Pixels moving_frame;
  first.setRenderTime(1000U + 360000U);
  first.render(moving_frame, black, warm_white);
  bool different_positions = false;
  for (uint8_t i = 1U; i < first.particleCount(); ++i) {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    first.explosionPosition(0U, 360000U, x0, y0);
    first.explosionPosition(i, 360000U, x1, y1);
    if (x0 != x1 || y0 != y1) different_positions = true;
  }
  assert(different_positions);
  assert(!equalsTranslated(moving_frame, old_mask));

  assert(first.update(1000U + clock_noon_effect::kExplosionUs).changed);
  assert(first.phase() == Phase::midpoint);
  assert(!first.ditherRequested());
  Pixels midpoint;
  first.render(midpoint, black, warm_white);
  for (uint8_t y = 0U; y < ws2812_display::logical_height; ++y) {
    for (uint8_t x = 0U; x < ws2812_display::logical_width; ++x) {
      assert(!on(midpoint, x, y));
    }
  }
  const uint32_t midpoint_end = 1000U + clock_noon_effect::kExplosionUs +
                                clock_noon_effect::kMidpointUs;
  assert(first.update(midpoint_end).changed);
  assert(first.phase() == Phase::reassembly);
  assert(first.ditherRequested());
  const Rgb reassembly_start = first.reassemblyColor(0U, 0U, warm_white);
  assert(reassembly_start.red == first_hot.red &&
         reassembly_start.green == first_hot.green &&
         reassembly_start.blue == first_hot.blue);
  const uint32_t before_landing_fade =
      clock_noon_effect::kReassemblyUs -
      clock_noon_effect::kReassemblyColorReturnUs - 10000U;
  for (uint8_t i = 0U; i < first.particleCount(); ++i) {
    const Rgb traveling = first.reassemblyColor(i, before_landing_fade, warm_white);
    const Rgb assigned = first.hotColor(i);
    assert(traveling.red == assigned.red && traveling.green == assigned.green &&
           traveling.blue == assigned.blue);
  }
  const Rgb landing_fade = first.reassemblyColor(
      0U, clock_noon_effect::kReassemblyUs -
              clock_noon_effect::kReassemblyColorReturnUs / 2U,
      warm_white);
  assert(landing_fade.red == 255U && landing_fade.green != first_hot.green);
  const Rgb reassembly_final_color = first.reassemblyColor(
      0U, clock_noon_effect::kReassemblyUs, warm_white);
  assert(reassembly_final_color.red == warm_white.red &&
         reassembly_final_color.green == warm_white.green &&
         reassembly_final_color.blue == warm_white.blue);
  Pixels assembling;
  first.setRenderTime(midpoint_end + 300000U);
  first.render(assembling, black, warm_white);
  clock_dissolve::ClockMask final_mask;
  assert(clock_dissolve::buildMask(final_mask, "12:00", new_x));
  assert(first.finalMask() == final_mask);
  assert(!matchesMask(assembling, final_mask));

  const uint32_t reassembly_end = midpoint_end + clock_noon_effect::kReassemblyUs;
  assert(first.update(reassembly_end).changed);
  assert(first.phase() == Phase::settle);
  assert(!first.ditherRequested());
  Pixels settled;
  first.render(settled, black, warm_white);
  assert(matchesMask(settled, final_mask));
  assert(!first.update(reassembly_end + clock_noon_effect::kSettleUs - 1U)
              .completed);
  const clock_noon_effect::AdvanceResult done =
      first.update(reassembly_end + clock_noon_effect::kSettleUs);
  assert(done.changed && done.completed && !first.active());
  assert(!first.ditherRequested());
  assert(ws2812_display::resolveProductionEffectDitherMode(
             first.ditherRequested()) == ws2812_display::TemporalDitherMode::off);
  assert(ws2812_display::resolveProductionEffectDitherMode(true) ==
         ws2812_display::TemporalDitherMode::perceptual);
}
}  // namespace

int main() { testIndependentDeterministicParticlesAndSequence(); }
