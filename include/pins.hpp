#pragma once
#include <cstdint>

// GPIO numbers, not physical header pin numbers. Directions are Pico-relative.
namespace pins {
constexpr uint8_t gps_tx = 0;
constexpr uint8_t gps_rx = 1;
constexpr uint8_t gps_pps = 2;
constexpr uint8_t vfd_tx = 4;
constexpr uint8_t vfd_rx_reserved = 5;
// GP6 and GP8-GP22 have no active CE assignment. GP8 is only a proposed
// future WS2812 output; removing its inherited SPI assignment does not
// implement or validate that output.
}
