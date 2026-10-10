#pragma once
#include <cstddef>
#include <cstdint>
constexpr int SERIAL_8N1 = 0;
constexpr int INPUT = 1;
class Print {
public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t byte) = 0;
    size_t write(const uint8_t* bytes, size_t count) {
        size_t written=0;
        while(written<count && write(bytes[written])==1) ++written;
        return written;
    }
};
class StubSerial : public Print {
public:
    using Print::write;
    size_t write(uint8_t) override { return 1; }
    int availableForWrite() const { return 64; }
    operator bool() const { return true; }
    void setTX(int value) { tx = value; }
    void setRX(int value) { rx = value; }
    void begin(unsigned long rate, int) { baud = rate; ++begin_count; }
    void flush() {}
    int tx = -1, rx = -1;
    unsigned long baud = 0;
    unsigned begin_count = 0;
};
static StubSerial Serial;
static StubSerial Serial1;
static StubSerial Serial2;
static int configured_pin = -1, configured_mode = -1;
inline void pinMode(int pin, int mode) { configured_pin = pin; configured_mode = mode; }
inline unsigned long millis() { return 0; }
inline void delay(unsigned long) {}
