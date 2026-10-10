#pragma once

#include "display_output.hpp"
#include "presentation_state.hpp"

namespace stdout_display {
// USB-CDC text adapter. It keeps one replaceable line and submits it atomically
// only when the bounded transport has room, avoiding interleaving with logs.
class Driver {
public:
    explicit Driver(display_output::Sink& sink) : sink_(sink) {}
    void begin(const presentation::State& state);
    void service(const presentation::State& state);

private:
    void format(const presentation::State& state);
    display_output::Sink& sink_;
    char line_[112] = {};
    unsigned length_ = 0;
    uint32_t next_update_us_ = 0;
    bool scheduled_ = false;
};
}
