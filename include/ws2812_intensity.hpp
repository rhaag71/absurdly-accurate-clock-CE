#pragma once

#include <cmath>
#include <cstdint>

// Local animation intensity only, not a global transfer curve or calibrated
// panel response. Stateless direct codes remain stable for identical inputs.
namespace ws2812_intensity {
inline double cosineEnvelope(uint32_t elapsed_us, uint32_t period_us) {
    if (period_us == 0) return 0.0;
    constexpr double pi = 3.14159265358979323846;
    const double phase = static_cast<double>(elapsed_us % period_us) / period_us;
    return (1.0 - std::cos(2.0 * pi * phase)) * 0.5;
}

// Gamma-shaped comparison: code = min + round((max-min) * intensity^2).
// Squaring allocates more time to low output codes. This is a provisional
// perceptual approximation, not a validated WS2812 low-code correction.
inline uint8_t channelCode(double intensity, uint8_t minimum, uint8_t maximum) {
    if (maximum < minimum) maximum = minimum;
    if (!(intensity > 0.0)) return minimum; // Includes invalid NaN input.
    if (intensity >= 1.0) return maximum;
    return static_cast<uint8_t>(minimum +
        (maximum - minimum) * intensity * intensity + 0.5);
}

// Collapse the low, visually unhelpful part of a fade to OFF, then remap the
// remaining envelope into useful stable device codes. The cutoff is inclusive
// so floating-point cosine noise at the boundary cannot flicker the pixel on.
inline uint8_t cutoffChannelCode(double intensity, double cutoff,
                                 uint8_t minimum, uint8_t maximum) {
    // Cosine values at mirrored cutoff phases may differ by a few floating
    // point ulps. Treat that numerical noise as exactly on the OFF boundary.
    constexpr double boundary_epsilon = 1e-12;
    if (!(intensity > cutoff + boundary_epsilon)) return 0;
    if (!(cutoff < 1.0)) return 0;
    const double active = (intensity - cutoff) / (1.0 - cutoff);
    return channelCode(active, minimum, maximum);
}
}
