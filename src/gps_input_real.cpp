#if defined(AAC_BUILD_PROFILE_PRODUCTION)
#include <Arduino.h>
#include "ce_config.hpp"
#include "gps_input_real.hpp"
#include "hardware.hpp"
#include "pins.hpp"

namespace {
gps_input::RealSource* active_source = nullptr;
void capturePpsInterrupt() {
    if (active_source) active_source->capturePps(micros());
}
}

namespace gps_input {
void RealSource::begin() {
    // Preserve startup FIFO handling; buffered bytes are discarded by main
    // until their reception timestamps can be trusted.
    Serial1.setFIFOSize(1024);
    hardware::begin(ce_config::gps_baud);
    active_source = this;
    attachInterrupt(digitalPinToInterrupt(pins::gps_pps), capturePpsInterrupt, RISING);
}

void RealSource::start(uint32_t) {}
void RealSource::poll(uint32_t) {}

clock_model::Pulse RealSource::snapshot(uint32_t& now_us) const {
    noInterrupts();
    clock_model::Pulse pulse;
    pulse.sequence = pps_count_;
    pulse.at_us = pps_at_us_;
    pulse.seen = pps_seen_;
    now_us = micros();
    interrupts();
    return pulse;
}

bool RealSource::readByte(uint32_t, ByteEvent& event) {
    if (Serial1.available() <= 0) return false;
    const int received = Serial1.read();
    if (received < 0) return false;
    // Match the original AAC sampling contract: PPS metadata and the
    // processing-time timestamp are sampled together. UART hardware does not
    // latch a per-character arrival timestamp.
    noInterrupts();
    event.pulse.sequence = pps_count_;
    event.pulse.at_us = pps_at_us_;
    event.pulse.seen = pps_seen_;
    event.timestamp_us = micros();
    interrupts();
    event.byte = static_cast<char>(received);
    return true;
}

bool RealSource::pending(uint32_t) const {
    return Serial1.available() > 0;
}

void RealSource::capturePps(uint32_t at_us) {
    pps_at_us_ = at_us;
    ++pps_count_;
    pps_seen_ = true;
}
}
#endif
