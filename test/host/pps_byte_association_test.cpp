#include <cassert>
#include <cstdint>
#include <type_traits>
#include "gps_input_event.hpp"
#include "gps_input_real.hpp"

namespace {
struct PpsState {
    uint32_t sequence = 0;
    uint32_t edge_us = 0;
    bool seen = false;
};

// Deterministic model of the real source's interrupt-masked sample. A pending
// edge can occur just before or just after this critical section, never between
// the copied PPS fields and processing timestamp.
gps_input::ByteEvent capture(char byte, uint32_t now_us,
                             const PpsState& state) {
    gps_input::ByteEvent event;
    event.byte = byte;
    event.pulse.sequence = state.sequence;
    event.pulse.at_us = state.edge_us;
    event.pulse.seen = state.seen;
    event.timestamp_us = now_us;
    return event;
}

void edgeImmediatelyAfterTimestampDoesNotMixSamples() {
    const uint32_t edge_us = 1000000;
    const uint32_t byte_processing_us = edge_us - 1;

    // The old split API could timestamp the '$', then observe this ISR before
    // taking a separate PPS snapshot, pairing a pre-edge time with seq 1.
    const uint32_t unsafe_timestamp = byte_processing_us;
    PpsState after_edge;
    after_edge.sequence = 1;
    after_edge.edge_us = edge_us;
    after_edge.seen = true;
    assert(unsafe_timestamp < after_edge.edge_us);
    assert(after_edge.sequence == 1);

    // With the coherent event, the interrupt is pending while the snapshot is
    // captured and runs only after all fields, including micros(), are copied.
    const PpsState before_edge;
    const auto event = capture('$', byte_processing_us, before_edge);
    assert(event.byte == '$');
    assert(event.timestamp_us < edge_us);
    assert(event.pulse.sequence == 0 && !event.pulse.seen);

    // Conversely, if the edge wins before sampling, all event fields describe
    // the new pulse. This is the boundary association used for RMC '$'.
    const auto after = capture('$', edge_us, after_edge);
    assert(after.timestamp_us >= after.pulse.at_us);
    assert(after.pulse.sequence == 1 && after.pulse.seen);
}

void realSourceReturnsOneAssociationRecord() {
    using Read = bool (gps_input::RealSource::*)(uint32_t,
                                                 gps_input::ByteEvent&);
    static_assert(std::is_same<decltype(&gps_input::RealSource::readByte), Read>::value,
                  "RealSource must return byte and association together");
}
}

int main() {
    edgeImmediatelyAfterTimestampDoesNotMixSamples();
    realSourceReturnsOneAssociationRecord();
}
