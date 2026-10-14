#include "dev_console.hpp"

#include <cstdio>
#include <cstring>
#include "clock_state.hpp"
#include "gps_input_simulated.hpp"
#if defined(AAC_DISPLAY_WS2812)
#include "display_transfer.hpp"
#include "ws2812_brightness.hpp"
#include "ws2812_clock_color.hpp"
#include "ws2812_test_pattern.hpp"
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
#include "ws2812_temporal_dither.hpp"
#endif
#endif

namespace dev_console {
namespace {
constexpr const char* fault_usage =
    "Usage: fault status | gps {on|off} | pps {on|off} | "
    "rmc {normal|bad-checksum|early|late} | clear";

bool whitespace(char c) { return c == ' ' || c == '\t'; }
bool digit(char c) { return c >= '0' && c <= '9'; }
unsigned pair(const char* value) {
    return static_cast<unsigned>((value[0] - '0') * 10 + value[1] - '0');
}
bool exactDigits(const char* value, size_t size) {
    for (size_t i = 0; i < size; ++i) if (!digit(value[i])) return false;
    return true;
}
#if defined(AAC_DISPLAY_WS2812)
bool parseTransferMode(const char* text, display_appearance::TransferMode& mode) {
    if (std::strcmp(text, "off") == 0) mode = display_appearance::TransferMode::off;
    else if (std::strcmp(text, "g2") == 0) mode = display_appearance::TransferMode::g2;
    else if (std::strcmp(text, "ws") == 0) mode = display_appearance::TransferMode::ws;
    else return false;
    return true;
}

bool parsePattern(const char* text, ws2812_display::TestPattern& pattern) {
    using ws2812_display::TestPattern;
    if (std::strcmp(text, "off") == 0 || std::strcmp(text, "clock") == 0)
        pattern = TestPattern::clock;
    else if (std::strcmp(text, "amber") == 0) pattern = TestPattern::amber;
    else if (std::strcmp(text, "red") == 0 || std::strcmp(text, "r") == 0)
        pattern = TestPattern::red;
    else if (std::strcmp(text, "green") == 0 || std::strcmp(text, "g") == 0)
        pattern = TestPattern::green;
    else if (std::strcmp(text, "blue") == 0 || std::strcmp(text, "b") == 0)
        pattern = TestPattern::blue;
    else if (std::strcmp(text, "white") == 0 || std::strcmp(text, "w") == 0)
        pattern = TestPattern::white;
    else if (std::strcmp(text, "cyan") == 0 || std::strcmp(text, "c") == 0)
        pattern = TestPattern::cyan;
    else if (std::strcmp(text, "magenta") == 0 || std::strcmp(text, "m") == 0)
        pattern = TestPattern::magenta;
    else return false;
    return true;
}

bool parseRamp(const char* text, ws2812_display::TestPattern& pattern) {
    using ws2812_display::TestPattern;
    if (std::strcmp(text, "r") == 0) pattern = TestPattern::ramp_red;
    else if (std::strcmp(text, "g") == 0) pattern = TestPattern::ramp_green;
    else if (std::strcmp(text, "b") == 0) pattern = TestPattern::ramp_blue;
    else if (std::strcmp(text, "gray") == 0) pattern = TestPattern::ramp_gray;
    else return false;
    return true;
}

bool parseProofMode(const char* text, ws2812_display::ProofMode& mode) {
    using ws2812_display::ProofMode;
    if (std::strcmp(text, "norm") == 0) mode = ProofMode::normal;
    else if (std::strcmp(text, "amber") == 0) mode = ProofMode::amber;
    else if (std::strcmp(text, "invert") == 0) mode = ProofMode::inverted;
    else if (std::strcmp(text, "bg") == 0) mode = ProofMode::background;
    else return false;
    return true;
}

bool parseRgb(const char* first, const char* remaining, int64_t& packed) {
    unsigned components[3] = {};
    const char* cursor = first;
    for (const char* p = cursor; *p; ++p) if (!digit(*p)) return false;
    if (!*cursor) return false;
    unsigned first_value = 0;
    for (; *cursor; ++cursor) {
        const unsigned digit_value = static_cast<unsigned>(*cursor - '0');
        if (first_value > (255u - digit_value) / 10u) return false;
        first_value = first_value * 10u + digit_value;
    }
    components[0] = first_value;
    cursor = remaining;
    for (unsigned component = 1; component < 3; ++component) {
        while (whitespace(*cursor)) ++cursor;
        if (!digit(*cursor)) return false;
        unsigned value = 0;
        while (digit(*cursor)) {
            const unsigned digit_value = static_cast<unsigned>(*cursor++ - '0');
            if (value > (255u - digit_value) / 10u) return false;
            value = value * 10u + digit_value;
        }
        components[component] = value;
        if (component == 1 && !whitespace(*cursor)) return false;
    }
    while (whitespace(*cursor)) ++cursor;
    if (*cursor) return false;
    packed = static_cast<int64_t>((components[0] << 16) |
                                  (components[1] << 8) | components[2]);
    return true;
}
#endif
bool leap(unsigned year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}
unsigned monthDays(unsigned year, unsigned month) {
    const unsigned days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return days[month - 1] + (month == 2 && leap(year) ? 1 : 0);
}
}

bool Console::pushResponse(char byte) {
    if (response_used_ == response_capacity) {
        ++response_drops_;
        return false;
    }
    responses_[response_head_] = byte;
    response_head_ = (response_head_ + 1) % response_capacity;
    ++response_used_;
    return true;
}

bool Console::pushDiagnostic(char byte) {
    if (diag_used_ == diagnostic_capacity) return false;
    diagnostics_[diag_head_] = byte;
    diag_head_ = (diag_head_ + 1) % diagnostic_capacity;
    ++diag_used_;
    return true;
}

void Console::queueDiagnostic(const char* message) {
    const size_t length = std::strlen(message);
    if (length > diagnostic_capacity - diag_used_) {
        diagnostic_drops_ += static_cast<uint32_t>(length);
        return; // Drop a whole diagnostic record, never a partial record.
    }
    for (size_t i = 0; i < length; ++i) pushDiagnostic(message[i]);
}

void Console::clearDiagnostics() {
    diagnostic_drops_ += static_cast<uint32_t>(diag_used_);
    diag_head_ = diag_tail_ = diag_used_ = 0;
}

bool Console::nextOutput(char& byte) {
    if (response_used_) {
        byte = responses_[response_tail_];
        response_tail_ = (response_tail_ + 1) % response_capacity;
        --response_used_;
        return true;
    }
    if (console_mode_ || diag_used_ == 0) return false;
    byte = diagnostics_[diag_tail_];
    diag_tail_ = (diag_tail_ + 1) % diagnostic_capacity;
    --diag_used_;
    return true;
}

void Console::write(const char* text) {
    while (*text) pushResponse(*text++);
}

void Console::writeLine(const char* text) {
    write(text);
    write("\r\n");
}

void writeFaultStatus(const gps_input::FaultControls& faults, Console& console) {
    console.writeLine(faults.gps_messages ? "GPS messages : ON" : "GPS messages : OFF");
    console.writeLine(faults.pps_output ? "PPS output   : ON" : "PPS output   : OFF");
    switch (faults.rmc_mode) {
    case gps_input::RmcMode::normal:
        console.writeLine("RMC mode     : NORMAL");
        break;
    case gps_input::RmcMode::bad_checksum:
        console.writeLine("RMC mode     : BAD-CHECKSUM");
        break;
    case gps_input::RmcMode::early:
        console.writeLine("RMC mode     : EARLY (5 ms after PPS)");
        break;
    case gps_input::RmcMode::late:
        console.writeLine("RMC mode     : LATE (880 ms after PPS)");
        break;
    }
}

void Console::prompt() { write("aac> "); }

void Console::enterConsole() {
    clearDiagnostics();
    console_mode_ = true;
    line_length_ = 0;
    line_overflow_ = false;
    write("\r\naac> ");
}

void Console::returnToMonitor() {
    clearDiagnostics(); // Never replay the bounded console-time backlog.
    console_mode_ = false;
    line_length_ = 0;
    line_overflow_ = false;
    writeLine("Returning to monitor mode");
}

bool Console::parseTime(const char* date, const char* time, int64_t& epoch) const {
    if (std::strlen(date) != 10 || date[4] != '-' || date[7] != '-' ||
        !exactDigits(date, 4) || !exactDigits(date + 5, 2) ||
        !exactDigits(date + 8, 2) || std::strlen(time) != 8 ||
        time[2] != ':' || time[5] != ':' || !exactDigits(time, 2) ||
        !exactDigits(time + 3, 2) || !exactDigits(time + 6, 2)) return false;

    const unsigned year = static_cast<unsigned>((date[0] - '0') * 1000 +
        (date[1] - '0') * 100 + (date[2] - '0') * 10 + date[3] - '0');
    const unsigned month = pair(date + 5);
    const unsigned day = pair(date + 8);
    const unsigned hour = pair(time);
    const unsigned minute = pair(time + 3);
    const unsigned second = pair(time + 6);
    // The RMC parser maps its two-digit year into 2000-2099.
    if (year < 2000 || year > 2099 || month < 1 || month > 12 ||
        day < 1 || day > monthDays(year, month) || hour > 23 ||
        minute > 59 || second > 59) return false;

    nmea::Utc utc;
    utc.year = static_cast<uint16_t>(year);
    utc.month = static_cast<uint8_t>(month);
    utc.day = static_cast<uint8_t>(day);
    utc.hour = static_cast<uint8_t>(hour);
    utc.minute = static_cast<uint8_t>(minute);
    utc.second = static_cast<uint8_t>(second);
    epoch = clock_model::toUnix(utc);
    return true;
}

void Console::execute(CommandHandler handler, void* context) {
    line_[line_length_] = '\0';
    char* begin = line_;
    while (whitespace(*begin)) ++begin;
    char* end = begin + std::strlen(begin);
    while (end > begin && whitespace(end[-1])) *--end = '\0';
    if (line_overflow_) {
        writeLine("Error: command too long");
    } else if (*begin) {
        char* args = begin;
        while (*args && !whitespace(*args)) ++args;
        if (*args) *args++ = '\0';
        while (whitespace(*args)) ++args;
        char* second = args;
        while (*second && !whitespace(*second)) ++second;
        if (*second) *second++ = '\0';
        while (whitespace(*second)) ++second;

        if (std::strcmp(begin, "help") == 0 && !*args) {
            write("help       Show commands\r\n");
            write("status     Show clock and simulator status\r\n");
            write("time       Set simulated UTC\r\n");
            write("           time YYYY-MM-DD HH:MM:SS\r\n");
#if defined(AAC_DISPLAY_WS2812)
            write("brightness Show or set WS2812 brightness (0-255)\r\n");
            write("           brightness <0-255>\r\n");
            write("color      Show or set logical clock RGB (0-255 each)\r\n");
            write("           color <r> <g> <b>\r\n");
            write("g          Show/set output transfer: off, g2, ws\r\n");
            write("           g {off|g2|ws}\r\n");
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
            write("dither     Show/set temporal output dithering (default off)\r\n");
            writeLine("           dither {off|basic|perceptual}");
#endif
            write("pat        Show/select full-panel test color; pat off restores clock\r\n");
            write("           pat {amber|r|g|b|w|c|m|off}\r\n");
            write("p          Show/select static clock proof: norm, amber, invert, bg\r\n");
            write("           p {norm|amber|invert|bg}\r\n");
            write("s          Overlay health proof: good, min, bad, off, live\r\n");
            write("           s {good|min|bad|off|live}\r\n");
            write("h          Overlay holdover proof: on, off (live)\r\n");
            write("           h {on|off}\r\n");
            write("lvl        Isolated stable red code: lvl <0-32|off>\r\n");
            write("day        Calendar marker proof: day 1-31, or live\r\n");
            write("           day <1-31|live>\r\n");
            writeLine("ramp       Show low-code ramp: ramp {r|g|b|gray} (0-31)");
#endif
            write("reset      Reset simulator to configured UTC seed\r\n");
            write("fault      Inspect or change simulated GPS/PPS faults\r\n");
            writeLine("           fault status | gps {on|off} | pps {on|off} | rmc {normal|bad-checksum|early|late} | clear");
            writeLine("exit       Return to diagnostic monitor");
        } else if (std::strcmp(begin, "status") == 0 && !*args) {
            if (handler) handler(context, Command::status, 0, *this);
        } else if (std::strcmp(begin, "reset") == 0 && !*args) {
            if (handler) handler(context, Command::reset, 0, *this);
        } else if (std::strcmp(begin, "time") == 0) {
            int64_t epoch = 0;
            if (!*args || !*second || !parseTime(args, second, epoch)) {
                writeLine("Usage: time YYYY-MM-DD HH:MM:SS (UTC, years 2000-2099)");
            } else if (handler) {
                handler(context, Command::set_time, epoch, *this);
            }
#if defined(AAC_DISPLAY_WS2812)
        } else if (std::strcmp(begin, "brightness") == 0) {
            if (!*args && !*second) {
                if (handler) handler(context, Command::brightness_status, 0, *this);
            } else if (*args && !*second && exactDigits(args, std::strlen(args))) {
                unsigned value = 0;
                for (const char* p = args; *p; ++p) {
                    value = value * 10u + static_cast<unsigned>(*p - '0');
                    if (value > 255u) break;
                }
                if (value <= 255u && handler)
                    handler(context, Command::brightness_set, value, *this);
                else
                    writeLine("Usage: brightness <0-255>");
            } else {
                writeLine("Usage: brightness <0-255>");
            }
        } else if (std::strcmp(begin, "color") == 0) {
            if (!*args && !*second) {
                if (handler) handler(context, Command::color_status, 0, *this);
            } else {
                int64_t packed = 0;
                if (parseRgb(args, second, packed) && handler)
                    handler(context, Command::color_set, packed, *this);
                else
                    writeLine("Usage: color <r> <g> <b> (each 0-255)");
            }
        } else if (std::strcmp(begin, "g") == 0) {
            if (!*args && !*second) {
                if (handler) handler(context, Command::transfer_status, 0, *this);
            } else {
                display_appearance::TransferMode mode;
                if (*args && !*second && parseTransferMode(args, mode) && handler)
                    handler(context, Command::transfer_set,
                            static_cast<int64_t>(mode), *this);
                else
                    writeLine("Usage: g {off|g2|ws}");
            }
#if defined(AAC_BUILD_PROFILE_DEVELOPMENT)
        } else if (std::strcmp(begin, "dither") == 0) {
            if (!*args && !*second) {
                if (handler) handler(context, Command::temporal_dither_status,
                                     0, *this);
            } else if (*args && !*second && handler) {
                if (std::strcmp(args, "off") == 0)
                    handler(context, Command::temporal_dither_set, 0, *this);
                else if (std::strcmp(args, "basic") == 0 ||
                         std::strcmp(args, "on") == 0)
                    handler(context, Command::temporal_dither_set, 1, *this);
                else if (std::strcmp(args, "perceptual") == 0)
                    handler(context, Command::temporal_dither_set, 2, *this);
                else
                    writeLine("Usage: dither {off|basic|perceptual}");
            } else {
                writeLine("Usage: dither {off|basic|perceptual}");
            }
#endif
        } else if (std::strcmp(begin, "pat") == 0) {
            if (!*args && !*second) {
                if (handler) handler(context, Command::pattern_status, 0, *this);
            } else {
                ws2812_display::TestPattern pattern;
                if (*args && !*second && parsePattern(args, pattern) && handler)
                    handler(context, Command::pattern_set,
                            static_cast<int64_t>(pattern), *this);
                else
                    writeLine("Usage: pat {amber|r|g|b|w|c|m|off}");
            }
        } else if (std::strcmp(begin, "ramp") == 0) {
            ws2812_display::TestPattern pattern;
            if (*args && !*second && parseRamp(args, pattern) && handler)
                handler(context, Command::ramp_set,
                        static_cast<int64_t>(pattern), *this);
            else
                writeLine("Usage: ramp {r|g|b|gray}");
        } else if (std::strcmp(begin, "p") == 0) {
            if (!*args && !*second) {
                if (handler) handler(context, Command::presentation_status, 0, *this);
            } else {
                ws2812_display::ProofMode mode;
                if (*args && !*second && parseProofMode(args, mode) && handler)
                    handler(context, Command::presentation_set,
                            static_cast<int64_t>(mode), *this);
                else
                    writeLine("Usage: p {norm|amber|invert|bg}");
            }
        } else if (std::strcmp(begin, "s") == 0) {
            if (*args && !*second) {
                if (std::strcmp(args, "off") == 0) {
                    if (handler) handler(context, Command::status_proof_set, 4, *this);
                } else if (std::strcmp(args, "good") == 0) {
                    if (handler) handler(context, Command::status_proof_set, 1, *this);
                } else if (std::strcmp(args, "min") == 0) {
                    if (handler) handler(context, Command::status_proof_set, 2, *this);
                } else if (std::strcmp(args, "bad") == 0) {
                    if (handler) handler(context, Command::status_proof_set, 3, *this);
                } else if (std::strcmp(args, "live") == 0) {
                    if (handler) handler(context, Command::status_proof_set, 0, *this);
                }
                else
                    writeLine("Usage: s {good|min|bad|off|live}");
            } else {
                writeLine("Usage: s {good|min|bad|off|live}");
            }
        } else if (std::strcmp(begin, "h") == 0) {
            if (*args && !*second &&
                (std::strcmp(args, "on") == 0 || std::strcmp(args, "off") == 0)) {
                if (handler) handler(context, Command::holdover_proof_set,
                    std::strcmp(args, "on") == 0 ? 1 : 0, *this);
            } else {
                writeLine("Usage: h {on|off}");
            }
        } else if (std::strcmp(begin, "lvl") == 0) {
            if (*args && !*second && std::strcmp(args, "off") == 0) {
                if (handler) handler(context, Command::level_proof_set, -1, *this);
            } else if (*args && !*second && exactDigits(args, std::strlen(args))) {
                unsigned value = 0;
                for (const char* p = args; *p; ++p) {
                    value = value * 10u + static_cast<unsigned>(*p - '0');
                    if (value > 32u) break;
                }
                if (value <= 32 && handler)
                    handler(context, Command::level_proof_set, value, *this);
                else writeLine("Usage: lvl <0-32|off>");
            } else writeLine("Usage: lvl <0-32|off>");
        } else if (std::strcmp(begin, "day") == 0) {
            if (*args && !*second && std::strcmp(args, "live") == 0) {
                if (handler) handler(context, Command::day_proof_set, 0, *this);
            } else if (*args && !*second && exactDigits(args, std::strlen(args))) {
                unsigned value = 0;
                for (const char* p = args; *p; ++p) {
                    value = value * 10u + static_cast<unsigned>(*p - '0');
                    if (value > 31u) break;
                }
                if (value >= 1 && value <= 31 && handler)
                    handler(context, Command::day_proof_set, value, *this);
                else
                    writeLine("Usage: day <1-31|live>");
            } else {
                writeLine("Usage: day <1-31|live>");
            }
#endif
        } else if (std::strcmp(begin, "exit") == 0 && !*args) {
            returnToMonitor();
        } else if (std::strcmp(begin, "fault") == 0) {
            if (std::strcmp(args, "status") == 0 && !*second) {
                if (handler) handler(context, Command::fault_status, 0, *this);
            } else if (std::strcmp(args, "clear") == 0 && !*second) {
                if (handler) handler(context, Command::fault_clear, 0, *this);
            } else {
                char* value = second;
                while (whitespace(*value)) ++value;
                char* end_value = value;
                while (*end_value && !whitespace(*end_value)) ++end_value;
                if (*end_value) *end_value++ = '\0';
                while (whitespace(*end_value)) ++end_value;
                if (!*value || *end_value) {
                    writeLine(fault_usage);
                } else if (std::strcmp(args, "gps") == 0 &&
                           std::strcmp(value, "off") == 0) {
                    if (handler) handler(context, Command::gps_off, 0, *this);
                } else if (std::strcmp(args, "gps") == 0 &&
                           std::strcmp(value, "on") == 0) {
                    if (handler) handler(context, Command::gps_on, 0, *this);
                } else if (std::strcmp(args, "pps") == 0 &&
                           std::strcmp(value, "off") == 0) {
                    if (handler) handler(context, Command::pps_off, 0, *this);
                } else if (std::strcmp(args, "pps") == 0 &&
                           std::strcmp(value, "on") == 0) {
                    if (handler) handler(context, Command::pps_on, 0, *this);
                } else if (std::strcmp(args, "rmc") == 0 &&
                           std::strcmp(value, "bad-checksum") == 0) {
                    if (handler) handler(context, Command::rmc_bad_checksum, 0, *this);
                } else if (std::strcmp(args, "rmc") == 0 &&
                           std::strcmp(value, "early") == 0) {
                    if (handler) handler(context, Command::rmc_early, 0, *this);
                } else if (std::strcmp(args, "rmc") == 0 &&
                           std::strcmp(value, "late") == 0) {
                    if (handler) handler(context, Command::rmc_late, 0, *this);
                } else if (std::strcmp(args, "rmc") == 0 &&
                           std::strcmp(value, "normal") == 0) {
                    if (handler) handler(context, Command::rmc_normal, 0, *this);
                } else {
                    writeLine(fault_usage);
                }
            }
        } else if (std::strcmp(begin, "help") == 0 ||
                   std::strcmp(begin, "status") == 0 ||
                   std::strcmp(begin, "reset") == 0 ||
                   std::strcmp(begin, "exit") == 0) {
            writeLine("Error: unexpected arguments");
        } else if (std::strcmp(begin, "fault") == 0) {
            writeLine(fault_usage);
        } else {
            write("Error: unknown command '");
            write(begin);
            writeLine("'");
        }
    }
    line_length_ = 0;
    line_overflow_ = false;
    line_[0] = '\0';
    if (console_mode_) prompt();
}

void Console::receive(char byte, CommandHandler handler, void* context) {
    if (ignore_lf_) {
        ignore_lf_ = false;
        if (byte == '\n') return;
    }
    if (!console_mode_) {
        if (byte == '\r' || byte == '\n') {
            ignore_lf_ = byte == '\r';
            enterConsole();
        }
        return;
    }

    if (byte == '\r' || byte == '\n') {
        ignore_lf_ = byte == '\r';
        write("\r\n");
        execute(handler, context);
        return;
    }
    if (byte == '\b' || byte == 0x7f) {
        if (line_length_ && !line_overflow_) {
            --line_length_;
            line_[line_length_] = '\0';
            write("\b \b");
        }
        return;
    }
    if (byte == '\t' || (byte >= 0x20 && byte <= 0x7e)) {
        if (!line_overflow_ && line_length_ < line_capacity - 1) {
            line_[line_length_++] = byte;
            line_[line_length_] = '\0';
            pushResponse(byte);
        } else {
            line_overflow_ = true;
        }
    }
}
}
