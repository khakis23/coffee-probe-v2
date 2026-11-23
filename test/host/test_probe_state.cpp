#include <doctest/doctest.h>

#include "core/probe_state.hpp"

TEST_CASE("A probe without a reading has no valid bean temperature") {
    const core::ProbeState state{};
    CHECK_FALSE(core::bt_valid(state));
}

TEST_CASE("Bean temperature is valid only while fresh and fault-free") {
    core::ProbeState state{};
    state.fault = core::TcFault::None;
    CHECK_FALSE(core::bt_valid(state));

    state.stale = false;
    CHECK(core::bt_valid(state));

    const core::TcFault faults[] = {core::TcFault::Open, core::TcFault::ShortGnd,
                                    core::TcFault::ShortVcc, core::TcFault::BusError};
    for (const auto fault : faults) {
        state.fault = fault;
        CHECK_FALSE(core::bt_valid(state));
    }
}
