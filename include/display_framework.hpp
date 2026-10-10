#pragma once

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>
#include "presentation_state.hpp"

namespace display_framework {
// Static composition: each selected driver owns its own state and is serviced
// once per application pass with the same read-only clock snapshot.
template <typename... Drivers>
class Framework {
public:
    template <typename... Args>
    explicit Framework(Args&&... args) : drivers_(std::forward<Args>(args)...) {}

    void begin(const presentation::State& state) { beginAt<0>(state); }
    void service(const presentation::State& state) { serviceAt<0>(state); }

private:
    template <std::size_t Index>
    typename std::enable_if<Index == sizeof...(Drivers)>::type
    beginAt(const presentation::State&) {}

    template <std::size_t Index>
    typename std::enable_if<(Index < sizeof...(Drivers))>::type
    beginAt(const presentation::State& state) {
        std::get<Index>(drivers_).begin(state);
        beginAt<Index + 1>(state);
    }

    template <std::size_t Index>
    typename std::enable_if<Index == sizeof...(Drivers)>::type
    serviceAt(const presentation::State&) {}

    template <std::size_t Index>
    typename std::enable_if<(Index < sizeof...(Drivers))>::type
    serviceAt(const presentation::State& state) {
        std::get<Index>(drivers_).service(state);
        serviceAt<Index + 1>(state);
    }

    std::tuple<Drivers...> drivers_;
};
}
