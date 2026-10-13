#pragma once

#include <cstdint>

// Presentation-only horizontal placement for the physical clock glyphs.
// Formatting and width measurement remain upstream; this state never owns or
// delays time content. Each eligible service advances at most one column so a
// delayed loop cannot replay stale animation steps in a burst.
namespace clock_recenter_slide {
constexpr uint32_t step_interval_us = 80000;

class State {
public:
    void reset() {
        initialized_ = false;
        active_ = false;
        current_x_ = 0;
        target_x_ = 0;
        last_step_us_ = 0;
    }

    // Returns true when placement changed or a new target started/stopped the
    // slide. Invalid time removes the prior-position history immediately.
    bool update(bool valid, int centered_x, uint32_t now_us) {
        if (!valid) {
            const bool changed = initialized_ || active_;
            reset();
            return changed;
        }

        if (!initialized_) {
            initialized_ = true;
            current_x_ = centered_x;
            target_x_ = centered_x;
            last_step_us_ = now_us;
            return true;
        }

        if (centered_x != target_x_) {
            target_x_ = centered_x;
            active_ = current_x_ != target_x_;
            last_step_us_ = now_us;
            return true;
        }

        if (!active_ ||
            static_cast<uint32_t>(now_us - last_step_us_) < step_interval_us)
            return false;

        current_x_ += current_x_ < target_x_ ? 1 : -1;
        active_ = current_x_ != target_x_;
        last_step_us_ = now_us;
        return true;
    }

    bool initialized() const { return initialized_; }
    bool active() const { return active_; }
    int currentX() const { return current_x_; }
    int targetX() const { return target_x_; }

private:
    bool initialized_ = false;
    bool active_ = false;
    int8_t current_x_ = 0;
    int8_t target_x_ = 0;
    uint32_t last_step_us_ = 0;
};
}
