#pragma once
#include "clock_display.hpp"
#include "pd2200.hpp"
#include "presentation_state.hpp"

namespace clock_display {
// Call only after the verified clear/home sequence. Does not transmit spaces.
void writeInitialFields(pd2200::Display& display, const Frame& initial);

// Optional observational record: valid only on successful payload acceptance.
// The position header was accepted earlier; this is not a display acknowledgment.
struct AcceptedCharacter {
    bool valid = false;
    uint8_t address = 0, payload = 0;
    // HH payloads are one transaction; complete is true on its final byte.
    bool hh_pair = false, complete = false;
    uint8_t hh[2] = {};
};
// One in-flight direct-position command, never an animation history queue.
// Each service call emits at most one byte when the UART reports writable.
class Output : private Print {
public:
    explicit Output(Print& uart) : uart_(uart), encoder_(*this) {}
    void reset(const Frame& displayed);
    void service(const Frame& desired, bool writable, AcceptedCharacter* accepted = nullptr);
    const Frame& submitted() const { return submitted_; }
private:
    size_t write(uint8_t byte) override;
    Print& uart_;
    pd2200::Display encoder_;
    Frame submitted_;
    uint8_t command_[5] = {};
    size_t size_ = 0, next_ = 0;
    uint8_t row_ = 0, column_ = 0;
};

// Compile-time-selected implementation of the presentation backend contract.
// PD-2200-specific rendering, command encoding, and UART submission stay here.
class Pd2200Backend {
public:
    Pd2200Backend();
    void begin(const presentation::State& state);
    void service(const presentation::State& state);
private:
    pd2200::Display display_;
    Output output_;
};
}
