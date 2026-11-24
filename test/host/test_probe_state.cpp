#include <doctest/doctest.h>

#include "core/probe_state.hpp"

TEST_CASE("A probe without a reading has no valid bean temperature") {
    const core::ProbeState state{};
    CHECK_FALSE(core::bt_valid(state, 0));
}

TEST_CASE("Bean temperature is valid only while fresh and fault-free") {
    core::ProbeState state{};
    state.fault = core::TcFault::None;
    CHECK_FALSE(core::bt_valid(state, 0));

    state.stale = false;
    CHECK(core::bt_valid(state, 0));

    const core::TcFault faults[] = {core::TcFault::Open, core::TcFault::ShortGnd,
                                    core::TcFault::ShortVcc, core::TcFault::BusError};
    for (const auto fault : faults) {
        state.fault = fault;
        CHECK_FALSE(core::bt_valid(state, 0));
    }
}

TEST_CASE("Bean temperature goes invalid by age even if the stale flag was never set") {
    core::ProbeState state{};
    state.fault = core::TcFault::None;
    state.stale = false;
    state.bt_updated_ms = UINT32_MAX - 99;  // 100 ms before the clock wraps.
    CHECK(core::bt_valid(state, 1900));     // 100 + 1900 = 2000 ms: the limit, still valid.
    CHECK_FALSE(core::bt_valid(state, 1901));
    CHECK(core::bt_stale(state, 1901));
}
