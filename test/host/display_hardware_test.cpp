#include <cassert>
#include <Arduino.h>
#include "hardware.hpp"
#include "pins.hpp"

// Compile the production hardware initializer in this translation unit so
// the stub UART observations below reflect the exact objects it touches.
#include "../../src/hardware.cpp"

int main() {
    hardware::begin(9600);
    assert(Serial1.tx == pins::gps_tx);
    assert(Serial1.rx == pins::gps_rx);
    assert(Serial1.baud == 9600);
    assert(Serial1.begin_count == 1);
    assert(Serial2.begin_count == 0); // Common hardware setup must not start VFD UART1.
    assert(configured_pin == pins::gps_pps);
    assert(configured_mode == INPUT);
}
