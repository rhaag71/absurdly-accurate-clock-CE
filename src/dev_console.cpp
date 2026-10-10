#include "dev_console.hpp"

#include <cstdio>
#include <cstring>
#include "clock_state.hpp"

namespace dev_console {
namespace {
bool whitespace(char c) { return c == ' ' || c == '\t'; }
bool digit(char c) { return c >= '0' && c <= '9'; }
unsigned pair(const char* value) {
    return static_cast<unsigned>((value[0] - '0') * 10 + value[1] - '0');
}
bool exactDigits(const char* value, size_t size) {
    for (size_t i = 0; i < size; ++i) if (!digit(value[i])) return false;
    return true;
}
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
            write("reset      Reset simulator to configured UTC seed\r\n");
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
        } else if (std::strcmp(begin, "exit") == 0 && !*args) {
            returnToMonitor();
        } else if (std::strcmp(begin, "help") == 0 ||
                   std::strcmp(begin, "status") == 0 ||
                   std::strcmp(begin, "reset") == 0 ||
                   std::strcmp(begin, "exit") == 0) {
            writeLine("Error: unexpected arguments");
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
