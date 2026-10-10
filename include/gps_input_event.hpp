#pragma once

#include <cstdint>
#include "clock_state.hpp"

namespace gps_input {
// Received byte plus the PPS/time sample associated with its processing.
// RealSource fills this from one interrupt-protected snapshot; the simulated
// source uses the byte's scheduled arrival and the PPS for that sentence.
struct ByteEvent {
    char byte = 0;
    clock_model::Pulse pulse;
    uint32_t timestamp_us = 0;
};
}
