#include <type_traits>
#include "display_selection.hpp"

#if defined(AAC_DISPLAY_STDOUT) && defined(AAC_DISPLAY_VFD)
static_assert(std::is_same<display_selection::Selected,
              display_framework::Framework<stdout_display::Driver,
                                 clock_display::Pd2200Backend>>::value,
              "explicit STDOUT+VFD selection composes both drivers");
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
