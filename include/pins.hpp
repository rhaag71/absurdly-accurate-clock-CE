#pragma once
#include <cstdint>

// GPIO numbers, not physical header pin numbers. Directions are Pico-relative.
namespace pins {
constexpr uint8_t gps_tx = 0;
constexpr uint8_t gps_rx = 1;
constexpr uint8_t gps_pps = 2;
constexpr uint8_t vfd_tx = 4;
constexpr uint8_t ws2812_data = 8;
// GP5-GP7 and GP9-GP22 have no active CE assignment. GP8 is owned only by
// the selected WS2812 driver; its external level shifting is hardware-side.
}
