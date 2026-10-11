#include <type_traits>
#include "display_selection.hpp"

#if defined(AAC_DISPLAY_STDOUT) && defined(AAC_DISPLAY_VFD)
#if defined(AAC_DISPLAY_WS2812)
static_assert(std::is_same<display_selection::Selected,
              display_framework::Framework<stdout_display::Driver,
                  clock_display::Pd2200Backend, ws2812_display::Driver>>::value,
              "all explicit drivers compose in requested combinations");
#else
static_assert(std::is_same<display_selection::Selected,
              display_framework::Framework<stdout_display::Driver,
                                 clock_display::Pd2200Backend>>::value,
              "explicit STDOUT+VFD selection composes both drivers");
#endif
#elif defined(AAC_DISPLAY_STDOUT) && defined(AAC_DISPLAY_WS2812)
static_assert(std::is_same<display_selection::Selected,
              display_framework::Framework<stdout_display::Driver,
                  ws2812_display::Driver>>::value,
              "explicit STDOUT+WS2812 selection composes both drivers");
#elif defined(AAC_DISPLAY_VFD) && defined(AAC_DISPLAY_WS2812)
static_assert(std::is_same<display_selection::Selected,
              display_framework::Framework<clock_display::Pd2200Backend,
                  ws2812_display::Driver>>::value,
              "explicit VFD+WS2812 selection composes both drivers");
#elif defined(AAC_DISPLAY_WS2812)
static_assert(std::is_same<display_selection::Selected,
              display_framework::Framework<ws2812_display::Driver>>::value,
              "explicit WS2812 selection must not add STDOUT");
#elif defined(AAC_DISPLAY_VFD)
static_assert(std::is_same<display_selection::Selected,
              display_framework::Framework<clock_display::Pd2200Backend>>::value,
              "VFD selection must not add STDOUT");
#else
static_assert(std::is_same<display_selection::Selected,
              display_framework::Framework<stdout_display::Driver>>::value,
              "no explicit driver defaults to STDOUT");
#endif

int main() {}
