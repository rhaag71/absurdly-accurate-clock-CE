#include <cassert>
#include "display_framework.hpp"

struct Observation {
    const presentation::State* begin_state = nullptr;
    const presentation::State* service_state = nullptr;
    unsigned begins = 0, services = 0;
};
struct MockDriver {
    Observation* observation;
    void begin(const presentation::State& state) {
        observation->begin_state = &state;
        ++observation->begins;
    }
    void service(const presentation::State& state) {
        observation->service_state = &state;
        ++observation->services;
    }
};

int main() {
    Observation first, second;
    display_framework::Framework<MockDriver, MockDriver> framework{
        MockDriver{&first}, MockDriver{&second}};
    presentation::State state;
    state.now_us = 123456;
    framework.begin(state);
    framework.service(state);
    assert(first.begins == 1 && second.begins == 1);
    assert(first.services == 1 && second.services == 1);
    assert(first.begin_state == &state && second.begin_state == &state);
    assert(first.service_state == &state && second.service_state == &state);
}
