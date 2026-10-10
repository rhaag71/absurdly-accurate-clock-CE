#include <cassert>
#include <cstring>
#include <type_traits>
#include "gps_input_source.hpp"

#if defined(AAC_BUILD_PROFILE_PRODUCTION)
static_assert(std::is_same<gps_input::Selected, gps_input::RealSource>::value,
              "Production must select only the real receiver source");
#else
static_assert(std::is_same<gps_input::Selected, gps_input::SimulatedSource>::value,
              "Development must select the simulated receiver source");
#endif

int main() {
#if defined(AAC_BUILD_PROFILE_PRODUCTION)
    assert(std::strcmp(gps_input::Selected::diagnosticLine(), "GPS_SOURCE=REAL\r\n") == 0);
#else
    assert(std::strcmp(gps_input::Selected::diagnosticLine(),
                       "GPS_SOURCE=SIMULATED\r\n") == 0);
#endif
}
