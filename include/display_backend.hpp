#pragma once

// Compile-time backend selection. A backend accepts presentation::State and
// owns all mapping, rendering/effects, and physical transport. It need not
// expose pixels, an RGB framebuffer, or any other backend-specific capability.
#if defined(AAC_DISPLAY_BACKEND_PD2200)
#include "clock_vfd.hpp"
namespace display_backend {
using Selected = clock_display::Pd2200Backend;
}
#else
#error "Select a supported AAC display backend at compile time"
#endif
