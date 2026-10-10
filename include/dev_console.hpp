#pragma once

#include <cstddef>
#include <cstdint>

namespace gps_input { struct FaultControls; }

namespace dev_console {
class Console;
enum class Command : uint8_t {
    status, set_time, reset,
    fault_status, gps_off, gps_on, pps_off, pps_on,
    rmc_bad_checksum, rmc_early, rmc_late, rmc_normal, fault_clear,
};
using CommandHandler = void (*)(void*, Command, int64_t, Console&);

class Console {
public:
    bool inConsole() const { return console_mode_; }
    bool hasPendingOutput() const { return response_used_ != 0 || (!console_mode_ && diag_used_ != 0); }
    uint32_t diagnosticBytesDropped() const { return diagnostic_drops_; }
    uint32_t responseBytesDropped() const { return response_drops_; }

    void queueDiagnostic(const char* message);
    bool nextOutput(char& byte);
    void receive(char byte, CommandHandler handler, void* context);
    void write(const char* text);
    void writeLine(const char* text);

private:
    static constexpr size_t diagnostic_capacity = 1024;
    static constexpr size_t response_capacity = 512;
    static constexpr size_t line_capacity = 80;

    void enterConsole();
    void returnToMonitor();
    void execute(CommandHandler handler, void* context);
    void prompt();
    void clearDiagnostics();
    bool pushResponse(char byte);
    bool pushDiagnostic(char byte);
    bool parseTime(const char* date, const char* time, int64_t& epoch) const;

    char line_[line_capacity] = {};
    size_t line_length_ = 0;
    char diagnostics_[diagnostic_capacity] = {};
    size_t diag_head_ = 0, diag_tail_ = 0, diag_used_ = 0;
    char responses_[response_capacity] = {};
    size_t response_head_ = 0, response_tail_ = 0, response_used_ = 0;
    uint32_t diagnostic_drops_ = 0;
    uint32_t response_drops_ = 0;
    bool console_mode_ = false;
    bool ignore_lf_ = false;
    bool line_overflow_ = false;
};

// Keep the receiver-control report consistent between firmware and host tests.
void writeFaultStatus(const gps_input::FaultControls& faults, Console& console);
}
