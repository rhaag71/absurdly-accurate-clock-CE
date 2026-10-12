#pragma once

#include <cstdint>

namespace display_appearance {
enum class TransferMode : uint8_t { off, g2, ws };

inline const char* transferModeName(TransferMode mode) {
    switch (mode) {
    case TransferMode::off: return "off";
    case TransferMode::g2: return "g2";
    case TransferMode::ws: return "ws";
    }
    return "off";
}

// `g2` is the standard inverse-gamma 2.0 comparison: round(sqrt(x * 255)).
inline uint8_t gamma2(uint8_t input) {
    const uint32_t value = static_cast<uint32_t>(input) * 255u;
    uint32_t low = 0;
    uint32_t high = 256;
    while (low + 1 < high) {
        const uint32_t middle = (low + high) / 2;
        if (middle * middle <= value) low = middle;
        else high = middle;
    }
    const uint32_t low_error = value - low * low;
    const uint32_t high_error = high * high - value;
    return static_cast<uint8_t>(high_error < low_error ? high : low);
}

// This illustrative WS2812-aware comparison uses a continuous ease-out curve
// that expands low codes and tapers to identity at full scale. It is motivated
// by reported low-code behavior, not measured or claimed to correct this panel.
inline uint8_t ws2812Comparison(uint8_t input) {
    const uint32_t lift =
        (static_cast<uint32_t>(input) * (255u - input) + 127u) / 255u;
    return static_cast<uint8_t>(input + lift);
}

inline uint8_t transferChannel(uint8_t input, TransferMode mode) {
    switch (mode) {
    case TransferMode::off: return input;
    case TransferMode::g2: return gamma2(input);
    case TransferMode::ws: return ws2812Comparison(input);
    }
    return input;
}

#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
inline TransferMode& transferModeStorage() {
    static TransferMode mode = TransferMode::off;
    return mode;
}
inline uint32_t& transferGenerationStorage() {
    static uint32_t generation = 0;
    return generation;
}
inline TransferMode transferMode() { return transferModeStorage(); }
inline void setTransferMode(TransferMode mode) {
    if (transferModeStorage() == mode) return;
    transferModeStorage() = mode;
    ++transferGenerationStorage();
}
inline uint32_t transferGeneration() { return transferGenerationStorage(); }

class TransferRefreshTracker {
public:
    TransferRefreshTracker() : observed_(transferGeneration()) {}
    void reset() { observed_ = transferGeneration(); }
    bool consumeChange() {
        const uint32_t current = transferGeneration();
        if (current == observed_) return false;
        observed_ = current;
        return true;
    }
private:
    uint32_t observed_;
};
#else
inline TransferMode transferMode() { return TransferMode::off; }
inline uint32_t transferGeneration() { return 0; }
class TransferRefreshTracker {
public:
    void reset() {}
    bool consumeChange() { return false; }
};
#endif
}
