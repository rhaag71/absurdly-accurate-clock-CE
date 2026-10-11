#pragma once

#include "display_framework.hpp"

// Define one or more AAC_DISPLAY_* flags. With no driver selected, STDOUT is
// the documented default; explicit physical selections never add it silently.
#if !defined(AAC_DISPLAY_STDOUT) && !defined(AAC_DISPLAY_VFD) && !defined(AAC_DISPLAY_WS2812)
#define AAC_DISPLAY_STDOUT 1
#define AAC_DISPLAY_DEFAULTED_TO_STDOUT 1
#endif

#if defined(AAC_DISPLAY_STDOUT)
#include "stdout_display.hpp"
#endif
#if defined(AAC_DISPLAY_VFD)
#include "clock_vfd.hpp"
#endif
#if defined(AAC_DISPLAY_WS2812)
#include "ws2812_display.hpp"
#endif

namespace display_selection {
#if defined(AAC_DISPLAY_STDOUT) && defined(AAC_DISPLAY_VFD) && defined(AAC_DISPLAY_WS2812)
using Selected = display_framework::Framework<stdout_display::Driver, clock_display::Pd2200Backend, ws2812_display::Driver>;
#elif defined(AAC_DISPLAY_STDOUT) && defined(AAC_DISPLAY_VFD)
using Selected = display_framework::Framework<stdout_display::Driver, clock_display::Pd2200Backend>;
#elif defined(AAC_DISPLAY_STDOUT) && defined(AAC_DISPLAY_WS2812)
using Selected = display_framework::Framework<stdout_display::Driver, ws2812_display::Driver>;
#elif defined(AAC_DISPLAY_VFD) && defined(AAC_DISPLAY_WS2812)
using Selected = display_framework::Framework<clock_display::Pd2200Backend, ws2812_display::Driver>;
#elif defined(AAC_DISPLAY_WS2812)
using Selected = display_framework::Framework<ws2812_display::Driver>;
#elif defined(AAC_DISPLAY_STDOUT)
using Selected = display_framework::Framework<stdout_display::Driver>;
#elif defined(AAC_DISPLAY_VFD)
using Selected = display_framework::Framework<clock_display::Pd2200Backend>;
#else
#error "No implemented display selected"
#endif
}

#undef AAC_DISPLAY_DEFAULTED_TO_STDOUT
