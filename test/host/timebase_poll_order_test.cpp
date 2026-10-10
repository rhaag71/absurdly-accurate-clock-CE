#include <cassert>
#include <cstdint>
#include "clock_state.hpp"
#include "monotonic_time.hpp"

namespace {
void backwardPollWouldUnderflowTimeoutInterval() {
    clock_model::Timebase unsafe;
    const nmea::Utc utc = clock_model::fromUnix(1767225600);
    const clock_model::Reception no_association;
    constexpr uint32_t last_rmc_us = 5000000;
    unsafe.receive(nmea::Result::valid_rmc, utc, 'A', no_association, last_rmc_us);
    assert(unsafe.state().gps_valid);

    const clock_model::Pulse no_pulse;
    unsafe.poll(no_pulse, last_rmc_us + 1000);
    assert(unsafe.state().gps_valid);

    // A scheduled simulator byte can be slightly earlier than the most recent
    // micros() poll. Passing it directly makes unsigned elapsed time enormous.
    const uint32_t scheduled_byte_us = last_rmc_us - 1000;
    unsafe.poll(no_pulse, scheduled_byte_us);
    assert(!unsafe.state().gps_valid); // Reproduces the false timeout.
}

void pollTimestampIsClampedButReceptionTimestampIsNot() {
    clock_model::Timebase safe;
    const nmea::Utc utc = clock_model::fromUnix(1767225600);
    const clock_model::Reception no_association;
    constexpr uint32_t last_rmc_us = 5000000;
    safe.receive(nmea::Result::valid_rmc, utc, 'A', no_association, last_rmc_us);
    const clock_model::Pulse no_pulse;
    uint32_t last_poll_us = last_rmc_us + 1000;
    safe.poll(no_pulse, last_poll_us);

    const uint32_t scheduled_byte_us = last_rmc_us - 1000;
    const uint32_t poll_us = clock_model::nondecreasingTimestamp(
        scheduled_byte_us, last_poll_us);
    safe.poll(no_pulse, poll_us);

    // Keep the generated time for the received data. Only poll servicing is
    // monotonic; source-provided RMC timing remains exact.
    assert(scheduled_byte_us == last_rmc_us - 1000);
    assert(poll_us == last_poll_us);
    assert(safe.state().gps_valid);
}

void comparisonHandlesMicrosRollover() {
    const uint32_t before_wrap = UINT32_MAX - 10;
    const uint32_t after_wrap = 5;
    assert(clock_model::nondecreasingTimestamp(after_wrap, before_wrap) == after_wrap);
    assert(clock_model::nondecreasingTimestamp(before_wrap, after_wrap) == after_wrap);
}
}

int main() {
    backwardPollWouldUnderflowTimeoutInterval();
    pollTimestampIsClampedButReceptionTimestampIsNot();
    comparisonHandlesMicrosRollover();
}
