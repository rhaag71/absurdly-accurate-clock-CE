#pragma once

#include <cstdint>

namespace clock_model {
// Keep timer servicing chronological while allowing unsigned micros() rollover.
// Valid for comparisons less than half the uint32_t timer range apart.
inline uint32_t nondecreasingTimestamp(uint32_t candidate_us,
                                      uint32_t previous_us) {
    return static_cast<int32_t>(candidate_us - previous_us) >= 0
        ? candidate_us : previous_us;
}
}
