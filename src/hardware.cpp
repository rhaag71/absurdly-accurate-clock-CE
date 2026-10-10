#include <Arduino.h>
#include "hardware.hpp"
#include "pins.hpp"

namespace hardware {
void begin(unsigned long gps_baud) {
    // GPS UART0 setup only. Display transports are initialized by their backend.
    Serial1.setTX(pins::gps_tx);
    Serial1.setRX(pins::gps_rx);
    Serial1.begin(gps_baud, SERIAL_8N1);
    pinMode(pins::gps_pps, INPUT);
}
}
