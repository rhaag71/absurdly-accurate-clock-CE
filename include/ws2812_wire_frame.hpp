#pragma once

#include "ws2812_pixels.hpp"
#include "ws2812_temporal_dither.hpp"

namespace ws2812_display {
constexpr uint32_t wire_bit_rate = 800000;
constexpr unsigned wire_cycles_per_bit = 10;
constexpr uint32_t wire_reset_us = 1000;
// DMA completion means the last word was queued, not sent. The joined FIFO
// holds eight words and the OSR can hold one more: allow all nine to drain.
constexpr uint32_t wire_tail_us = 9 * 24 * 1000000u / wire_bit_rate;

// One prepared GRB stream, not another logical RGB framebuffer. DMA receives
// only a const pointer. Preparation/paint/start are refused while it owns the
// stream, including FIFO drain and the subsequent reset-low interval.
class WireFrame {
public:
    bool prepare(const Pixels& pixels, uint8_t brightness,
                 display_appearance::TransferMode transfer) {
        if (busy_) return false;
        for (std::size_t i = 0; i < pixel_count; ++i)
            words_[i] = pixels.grbWord(i, brightness, transfer);
        prepared_ = true;
        return true;
    }

    bool prepareDithered(const Pixels& pixels, uint8_t brightness,
                         display_appearance::TransferMode transfer,
                         TemporalDither& dither,
                         TemporalDitherMode mode = TemporalDitherMode::basic) {
        if (busy_) return false;
        for (std::size_t i = 0; i < pixel_count; ++i) {
            const Rgb scaled = dither.scale(pixels.at(i), brightness, i, mode);
            words_[i] = pixels.grbWordForColor(scaled, 255U, transfer);
        }
        prepared_ = true;
        return true;
    }

    // Final output RGB is already in device code units. No brightness or
    // transfer transform remains in the serializer/DMA path.
    bool paintOutputPixel(int x, int y, Rgb color) {
        uint16_t raw;
        if (busy_ || !prepared_ || !logicalToRaw(x, y, raw)) return false;
        words_[raw] = (uint32_t(color.green) << 24) |
                      (uint32_t(color.red) << 16) | (uint32_t(color.blue) << 8);
        return true;
    }

    bool start() {
        if (busy_ || !prepared_) return false;
        busy_ = true;
        draining_ = false;
        return true;
    }

    bool poll(uint32_t now_us, bool dma_busy) {
        if (!busy_) return true;
        if (!draining_) {
            if (dma_busy) return false;
            deadline_us_ = now_us + wire_tail_us + wire_reset_us;
            draining_ = true;
        }
        if (static_cast<int32_t>(now_us - deadline_us_) < 0) return false;
        busy_ = false;
        prepared_ = false;
        return true;
    }

    bool busy() const { return busy_; }
    const uint32_t* words() const { return words_; }

private:
    uint32_t words_[pixel_count] = {};
    uint32_t deadline_us_ = 0;
    bool prepared_ = false;
    bool busy_ = false;
    bool draining_ = false;
};
}
