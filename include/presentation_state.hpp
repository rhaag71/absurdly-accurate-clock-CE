#pragma once

#include "clock_state.hpp"
#include "display_time.hpp"

// Immutable-by-convention value snapshot for display backends. Backends may
// format or animate this data, but cannot write to the authoritative timebase.
namespace presentation {
struct State {
    clock_model::State clock;
    clock_model::Pulse pulse;
    uint32_t now_us = 0;
    DisplayZone zone = DisplayZone::utc;
};
}
