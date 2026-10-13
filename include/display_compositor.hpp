#pragma once

#include "compact_clock_font.hpp"
#include "display_appearance.hpp"

namespace display_compositor {
// Opaque black is a paint operation. Transparent paint is a no-op.
struct Paint {
    bool opaque;
    display_appearance::Rgb color;
};

inline Paint transparent() { return {false, {0, 0, 0}}; }
inline Paint solid(display_appearance::Rgb color) { return {true, color}; }

template <typename Frame>
inline void beginFrame(Frame& frame, display_appearance::Rgb background) {
    frame.fill(background);
}

// The font supplies only the glyph mask; paint is applied to set mask pixels.
// Unset mask pixels preserve the background, including when paint is black.
template <typename Frame>
inline bool drawClockLayerAt(Frame& frame, const char* text, int x, int y,
                             Paint paint) {
    if (!paint.opaque) return true;
    return compact_clock_font::drawText(frame, text, x, y, paint.color);
}

template <typename Frame>
inline bool drawClockLayer(Frame& frame, const char* text, int panel_width,
                           int y, Paint paint) {
    if (!paint.opaque) return true;
    const int width = compact_clock_font::textWidth(text);
    if (width < 0 || width > panel_width) return false;
    return drawClockLayerAt(frame, text, (panel_width - width) / 2, y, paint);
}

// Apply an overlay pixel after background and clock. Transparent leaves the
// composed pixel untouched; any opaque RGB value, including black, replaces it.
template <typename Frame>
inline bool applyOverlayPixel(Frame& frame, int x, int y, Paint paint) {
    return !paint.opaque || frame.setLogical(x, y, paint.color);
}

template <typename Frame>
inline bool composeClock(Frame& frame, const char* text, int panel_width,
                         int y, display_appearance::Rgb background,
                         Paint clock_paint) {
    beginFrame(frame, background);
    return drawClockLayer(frame, text, panel_width, y, clock_paint);
}

template <typename Frame>
inline bool composeClockAt(Frame& frame, const char* text, int x, int y,
                           display_appearance::Rgb background,
                           Paint clock_paint) {
    beginFrame(frame, background);
    return drawClockLayerAt(frame, text, x, y, clock_paint);
}

template <typename Frame, typename OverlayLayer>
inline bool compose(Frame& frame, const char* text, int panel_width, int y,
                    display_appearance::Rgb background, Paint clock_paint,
                    OverlayLayer overlay) {
    if (!composeClock(frame, text, panel_width, y, background, clock_paint))
        return false;
    return overlay(frame);
}
}
