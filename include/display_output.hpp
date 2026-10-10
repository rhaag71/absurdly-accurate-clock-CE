#pragma once

#include <cstddef>
#include <cstdint>

namespace display_output {
// A bounded nonblocking sink keeps output policy and transport ownership
// outside display drivers. `quiet` discards a pending frame and retries from
// current presentation state when output is enabled again.
enum class Availability : uint8_t { ready, blocked, quiet };

class Sink {
public:
    virtual ~Sink() {}
    virtual Availability availability(size_t required) const = 0;
    virtual bool write(const uint8_t* bytes, size_t length) = 0;
};
}
