#include <cassert>

#include "ws2812_temporal_dither.hpp"

using namespace ws2812_display;

static_assert(sizeof(TemporalDither) == pixel_count * 6U + 2U,
              "production dither state remains fixed and bounded");
static_assert(sizeof(TemporalDitherRefreshSchedule) <= 12U,
              "production scheduling excludes development telemetry");
static_assert(resolveProductionEffectDitherMode(false) ==
                  TemporalDitherMode::off,
              "normal production presentation does not dither");
static_assert(resolveProductionEffectDitherMode(true) ==
                  TemporalDitherMode::perceptual,
              "production effects cannot select basic dithering");

int main() {
  assert(temporalDitherMode() == TemporalDitherMode::off);
  TemporalDitherRefreshSchedule schedule;
  assert(schedule.rateHz() == effect_temporal_dither_rate_hz);
  assert(schedule.periodUs() == 9090U);
  schedule.reset(100U);
  assert(!schedule.due(9189U));
  assert(schedule.due(9190U));
  schedule.stop();
  assert(!schedule.due(1000000U));
}
