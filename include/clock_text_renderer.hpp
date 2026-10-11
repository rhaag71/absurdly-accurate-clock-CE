#pragma once

#include "ce_config.hpp"
#include "compact_clock_font.hpp"
#include "presentation_state.hpp"

// Civil conversion stays in presentation; this renderer only formats the
// authoritative snapshot for the primary compact logical clock font.
namespace clock_text_renderer {
inline bool formatHHMM(const presentation::State& state, char (&text)[6],
                       ce_config::HourFormat hour_format = ce_config::hour_format) {
    if (!state.clock.utc_valid) return false;

    const presentation::DisplayTime local =
        presentation::convertUtcForDisplay(state.clock.utc, state.zone);
    unsigned hour = local.civil.hour;
    if (hour_format == ce_config::HourFormat::twelve_hour) {
        hour %= 12;
        if (hour == 0) hour = 12;
    }

    int position = 0;
    if (hour_format == ce_config::HourFormat::twenty_four_hour || hour >= 10)
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
                   const Color& color,
                   ce_config::HourFormat hour_format = ce_config::hour_format) {
    frame.clear();
    char text[6];
    if (!formatHHMM(state, text, hour_format)) return false;
    return compact_clock_font::drawCenteredText(
        frame, text, 32, 1, color);
}
}
