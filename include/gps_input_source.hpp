#pragma once

// The profile selects exactly one source at compile time. Both expose the same
// small ingress API; parsers and the authoritative Timebase remain shared.
#if defined(AAC_BUILD_PROFILE_PRODUCTION) && !defined(AAC_BUILD_PROFILE_DEVELOPMENT)
#include "gps_input_real.hpp"
namespace gps_input { using Selected = RealSource; }
#elif defined(AAC_BUILD_PROFILE_DEVELOPMENT) && !defined(AAC_BUILD_PROFILE_PRODUCTION)
#include "gps_input_simulated.hpp"
namespace gps_input { using Selected = SimulatedSource; }
#else
#error "Select exactly one AAC-CE GPS input source profile"
#endif
