#pragma once

#include "compact_clock_font.hpp"
#include "presentation_state.hpp"

// Civil conversion stays in presentation; this renderer formats the
// authoritative snapshot for the primary 5x7 clock glyph masks.
namespace clock_text_renderer {
inline bool formatHHMM(const presentation::State& state, char (&text)[6]) {
    if (!state.clock.utc_valid) return false;

    const presentation::DisplayTime local =
        presentation::convertUtcForDisplay(state.clock.utc, state.zone);
    unsigned hour = local.civil.hour;
    // The WS2812 clock face is a consumer 12-hour display in every profile.
    // Keep UTC and any other 24-hour representations in state/diagnostics.
    hour %= 12;
    if (hour == 0) hour = 12;

    int position = 0;
    if (hour >= 10)
        text[position++] = static_cast<char>('0' + hour / 10);
    text[position++] = static_cast<char>('0' + hour % 10);
    text[position++] = ':';
    text[position++] = static_cast<char>('0' + local.civil.minute / 10);
    text[position++] = static_cast<char>('0' + local.civil.minute % 10);
    text[position] = '\0';
    if (position + 1 < 6) text[position + 1] = '\0';
    return true;
}

template <typename LogicalFrame, typename Color>
inline bool render(const presentation::State& state, LogicalFrame& frame,
                   const Color& color) {
    frame.clear();
    char text[6];
    if (!formatHHMM(state, text)) return false;
    return compact_clock_font::drawCenteredText(
        frame, text, 32, 0, color);
}
}
