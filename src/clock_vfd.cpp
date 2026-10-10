#if defined(AAC_DISPLAY_VFD)
#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include "clock_vfd.hpp"
#include "pins.hpp"

namespace clock_display {
Pd2200Backend::Pd2200Backend() : display_(Serial2), output_(Serial2) {}

void writeInitialFields(pd2200::Display& display, const Frame& initial) {
    display.writeField(0, 3, initial.rows[0] + 3, 3);  // centered UTC
    display.writeField(0, 7, initial.rows[0] + 7, 10); // HH:MM:SS.X
    display.writeField(1, 0, initial.rows[1], 3);       // GPS
    display.writeChar(1, 3, initial.rows[1][3]);
    display.writeField(1, 6, initial.rows[1] + 6, 3);  // PPS
    display.writeChar(1, 9, initial.rows[1][9]);
    display.writeField(1, 12, initial.rows[1] + 12, 3); // SAT label
    display.writeChar(1, 16, initial.rows[1][16]);
    display.writeChar(1, 17, initial.rows[1][17]);
}
void Output::reset(const Frame& displayed) {
    submitted_ = displayed;
    size_ = next_ = 0;
}
size_t Output::write(uint8_t byte) {
    if (size_ == sizeof(command_)) return 0;
    command_[size_++] = byte;
    return 1;
}
void Output::service(const Frame& desired, bool writable, AcceptedCharacter* accepted) {
    if (accepted) accepted->valid = false;
    if (!writable) return; // No command selection/queueing while backpressured.
    if (next_ == 0) {
        const auto update = difference(submitted_, desired);
        bool found = false;
        // Zone label and clock digits before indicator, then status.
        for (uint8_t row = 0; row < 2 && !found; ++row) {
            for (uint8_t col = 0; col < columns; ++col) {
                if (update.positions[row] & (1u << col)) {
                    row_ = row;
                    column_ = col;
                    found = true;
                    break;
                }
            }
        }
        if (!found) return;
        size_ = 0;
        if (row_ == 0 && (column_ == 7 || column_ == 8)) {
            // PD-2200 accommodation: never directly address HH ones at 08.
            // The physical x0 -> x8 root cause remains unproven.
            column_ = 7;
            encoder_.writeField(0, 7, desired.rows[0] + 7, 2);
        } else {
            encoder_.writeChar(row_, column_, desired.rows[row_][column_]);
        }
    }
    const bool hh_pair = size_ == 5;
    // Refresh before the first payload is accepted, including on retries.
    // Once HH tens is accepted, freeze BOTH payloads until ones is accepted.
    // A later desired change is picked up as another complete pair afterward.
    if (next_ == 3) {
        const char value = desired.rows[row_][column_];
        const char ones = hh_pair ? desired.rows[0][8] : 0;
        if (!hh_pair && row_ == 0 && column_ == decade_column && value == '0') {
            // At a PPS boundary, do not refresh an in-flight decade header to
            // zero ahead of the new second. Let changed clock digits submit
            // first; Output's normal left-to-right selection sends zero next.
            for (uint8_t col = 7; col <= 14; ++col) {
                if (desired.rows[0][col] != submitted_.rows[0][col]) {
                    size_ = next_ = 0; // Header is complete; a new address follows.
                    return;
                }
            }
        }
        const bool unchanged = value == submitted_.rows[row_][column_] &&
                               (!hh_pair || ones == submitted_.rows[0][8]);
        if (unchanged || value == ' ' || (hh_pair && ones == ' ')) {
            // The position header is complete; no payload has been accepted.
            size_ = next_ = 0;
            return;
        }
        command_[3] = static_cast<uint8_t>(value);
        if (hh_pair) command_[4] = static_cast<uint8_t>(ones);
    }
    if (uart_.write(command_[next_]) != 1) return;
    if (next_ >= 3) {
        const uint8_t column = column_ + next_ - 3;
        // Cache each byte actually accepted, including a partially sent HH pair.
        submitted_.rows[row_][column] = static_cast<char>(command_[next_]);
        if (accepted) {
            accepted->valid = true;
            accepted->address = row_ * columns + column;
            accepted->payload = command_[next_];
            accepted->hh_pair = hh_pair;
            accepted->complete = next_ + 1 == size_;
            if (hh_pair) {
                accepted->hh[0] = command_[3];
                accepted->hh[1] = command_[4];
            }
        }
    }
    if (++next_ == size_) size_ = next_ = 0;
}

void Pd2200Backend::begin(const presentation::State& state) {
    // PD-2200 physical transport: UART1 TX on GP4, transmit-only, 9600 8N1.
    Serial2.setTX(pins::vfd_tx);
    Serial2.setRX(-1); // UART1 is transmit-only; the VFD claims no RX GPIO.
    Serial2.begin(9600, SERIAL_8N1);
    // Preserve the separately powered display's settling interval without
    // stalling GPS/PPS servicing or watchdog progress in the main loop.
    (void)state;
    output_.reset(Frame{});
    startup_at_ms_ = millis();
    startup_ = Startup::settling;
}

void Pd2200Backend::service(const presentation::State& state) {
    const uint32_t now_ms = millis();
    if (startup_ == Startup::settling) {
        if (static_cast<uint32_t>(now_ms - startup_at_ms_) < 500 ||
            Serial2.availableForWrite() < 2) return;
        display_.beginReset();
        startup_at_ms_ = now_ms;
        startup_ = Startup::reset_wait;
        return;
    }
    if (startup_ == Startup::reset_wait) {
        if (static_cast<uint32_t>(now_ms - startup_at_ms_) < 100 ||
            Serial2.availableForWrite() < 6) return;
        startup_ = Startup::configure;
    }
    if (startup_ == Startup::configure) {
        display_.configure();
        startup_ = Startup::active;
        return;
    }
    const auto desired = render(state.clock, state.pulse, state.now_us, state.zone);
#if !defined(AAC_DISPLAY_STDOUT)
    static char last_hh[2] = {}, last_label[3] = {};
    const bool changed = std::memcmp(last_hh, desired.rows[0] + 7, 2) != 0 ||
                         std::memcmp(last_label, desired.rows[0] + 3, 3) != 0;
    if (changed) {
        std::memcpy(last_hh, desired.rows[0] + 7, 2);
        std::memcpy(last_label, desired.rows[0] + 3, 3);
        const auto& utc = state.clock.utc;
        const auto local = state.clock.utc_valid
            ? presentation::convertUtcForDisplay(utc, state.zone)
            : presentation::DisplayTime{utc, "---", 0, false};
        char message[128];
        const int length = std::snprintf(message, sizeof(message),
            "HH ms=%lu pps=%lu valid=%u utc=%02u:%02u:%02u zone=%s "
            "civil=%02u:%02u:%02u want=%.2s cache=%.2s\r\n",
            static_cast<unsigned long>(millis()),
            static_cast<unsigned long>(state.pulse.sequence),
            static_cast<unsigned>(state.clock.utc_valid), utc.hour, utc.minute,
            utc.second, presentation::zoneName(state.zone), local.civil.hour,
            local.civil.minute, local.civil.second, desired.rows[0] + 7,
            output_.submitted().rows[0] + 7);
        // Diagnostics are best-effort and must never block display servicing.
        if (length > 0 && static_cast<size_t>(length) < sizeof(message) && Serial &&
            Serial.availableForWrite() >= length)
            Serial.write(reinterpret_cast<const uint8_t*>(message), length);
    }
#endif

    AcceptedCharacter accepted;
    output_.service(desired, Serial2.availableForWrite() > 0, &accepted);
#if !defined(AAC_DISPLAY_STDOUT)
    if (accepted.valid && accepted.hh_pair && accepted.complete) {
        char message[112];
        const int length = std::snprintf(message, sizeof(message),
            "TXHH ms=%lu pps=%lu kind=pair bytes=1B4807%02X%02X want=%.2s cache=%.2s\r\n",
            static_cast<unsigned long>(millis()),
            static_cast<unsigned long>(state.pulse.sequence), accepted.hh[0],
            accepted.hh[1], desired.rows[0] + 7, output_.submitted().rows[0] + 7);
        if (length > 0 && static_cast<size_t>(length) < sizeof(message) && Serial &&
            Serial.availableForWrite() >= length)
            Serial.write(reinterpret_cast<const uint8_t*>(message), length);
    }
#endif
}
}
#endif
