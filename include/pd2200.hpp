#pragma once
#include <Arduino.h>

// PD-2200 is configured externally for Noritake serial command mode, 9600 baud,
// 8N1. This class encodes the display commands; UART/RS-232 electrical
// transport is configured separately in hardware.cpp and outside this class.
namespace pd2200 {
constexpr size_t columns = 20;

class Display {
public:
    explicit Display(Print& uart) : uart_(uart) {}
    // UART must already be initialized and display power stable.
    void begin();
    // In the verified PD-2200 behavior, Noritake command 0x0E clears displayed
    // characters without resetting the current write/cursor position. Home
    // (0x0C) is a separate command. Do not generalize this to other displays.
    // Use for initialization or major layout changes only.
    void clear();
    // Zero-based direct position. Returns false without writing for an invalid cell.
    bool position(uint8_t row, uint8_t column);
    // At most length characters, bounded to this row; no terminator/padding sent.
    void writeField(uint8_t row, uint8_t column, const char* text, size_t length);
    // Zero-based row; exactly 20 characters plus a terminator in the buffer.
    // Sends 20 printable ASCII cells; a short string is padded with spaces.
    // Terminators/control bytes never become visible content.
    void writeRow(uint8_t row, const char (&text)[columns + 1]);

    // Direct-position update using the same ESC H addressing as writeRow.
    void writeChar(uint8_t row, uint8_t column, char value);

private:
    Print& uart_;
};
}
