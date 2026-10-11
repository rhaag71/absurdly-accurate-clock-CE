#pragma once

#include "ce_config.hpp"
#include "numeric_font_5x7.hpp"
#include "presentation_state.hpp"

// Civil conversion stays in presentation; this renderer only formats the
// authoritative snapshot for the temporary logical 5x7 clock proof.
namespace clock_text_renderer {
inline bool formatHHMM(const presentation::State& state, char (&text)[6]) {
    if (!state.clock.utc_valid) return false;

    const presentation::DisplayTime local =
        presentation::convertUtcForDisplay(state.clock.utc, state.zone);
    unsigned hour = local.civil.hour;
    if (ce_config::hour_format == ce_config::HourFormat::twelve_hour) {
        hour %= 12;
        if (hour == 0) hour = 12;
    }

    text[0] = static_cast<char>('0' + hour / 10);
    text[1] = static_cast<char>('0' + hour % 10);
    text[2] = ':';
    text[3] = static_cast<char>('0' + local.civil.minute / 10);
    text[4] = static_cast<char>('0' + local.civil.minute % 10);
    text[5] = '\0';
    return true;
}

template <typename LogicalFrame, typename Color>
inline bool render(const presentation::State& state, LogicalFrame& frame,
                   const Color& color) {
    frame.clear();
    char text[6];
    if (!formatHHMM(state, text)) return false;
    return numeric_font_5x7::drawText(frame, text, 3, 0, color);
}
}
