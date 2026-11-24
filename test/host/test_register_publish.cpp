#include "core/register_publish.hpp"

#include <doctest/doctest.h>

TEST_CASE("register publication maps a valid reading and system fields") {
    core::ProbeState state;
    state.bt = {1000};
    state.fault = core::TcFault::None;
    state.stale = false;
    state.cold_junction = core::DeciC{-15};
    state.uptime_s = 0x00012345;

    const auto regs = core::build_registers(state, 0, 0x0102).regs;
    // 100 * 9/5 + 32 = 212 F. Cold junction is -1.5 C, or -15 in tenths.
    CHECK(regs[core::reg::kBtF10] == 2120);
    CHECK(regs[core::reg::kBtC10] == 1000);
    CHECK(regs[core::reg::kStatus] == 0);
    CHECK(regs[core::reg::kDieC10] == 0x8000);
    CHECK(regs[core::reg::kColdJunctionC10] == 0xFFF1);
    CHECK(regs[core::reg::kBatteryPct] == 0xFFFF);
    CHECK(regs[core::reg::kUptimeHi] == 0x0001);
    CHECK(regs[core::reg::kUptimeLo] == 0x2345);
    CHECK(regs[core::reg::kFirmwareVersion] == 0x0102);
}

TEST_CASE("each probe fault publishes raw minus ten and its status bit") {
    struct FaultCase {
        core::TcFault fault;
        uint16_t status;
    };
    // Register map bits 0, 1 and 6 are open, short and bus error.
    for (const auto [fault, status] : {FaultCase{core::TcFault::Open, 0x01},
                                      FaultCase{core::TcFault::ShortGnd, 0x02},
                                      FaultCase{core::TcFault::ShortVcc, 0x02},
                                      FaultCase{core::TcFault::BusError, 0x40}}) {
        core::ProbeState state;
        state.bt = {1000};
        state.fault = fault;
        state.stale = false;
        const auto regs = core::build_registers(state, 0, 0).regs;
        CHECK(regs[core::reg::kBtC10] == 0xFFF6);
        CHECK(regs[core::reg::kBtF10] == 0xFFF6);
        CHECK(regs[core::reg::kStatus] == status);
    }
}

TEST_CASE("stale data publishes the fault value even with no probe fault") {
    core::ProbeState state;
    state.bt = {1000};
    state.fault = core::TcFault::None;
    state.stale = true;
    const auto regs = core::build_registers(state, 0, 0).regs;
    CHECK(regs[core::reg::kBtC10] == 0xFFF6);
    CHECK(regs[core::reg::kBtF10] == 0xFFF6);
    CHECK(regs[core::reg::kStatus] == 0x04);  // Stale is bit 2.
    CHECK(regs[core::reg::kColdJunctionC10] == 0x8000);

    state.fault = core::TcFault::Open;
    CHECK(core::build_registers(state, 0, 0).regs[core::reg::kStatus] == 0x05);
}

TEST_CASE("an old reading publishes the fault value even if the state still says fresh") {
    // The sensor task froze after a good reading at 1000 ms: stale was never set.
    core::ProbeState state;
    state.bt = {1000};
    state.fault = core::TcFault::None;
    state.stale = false;
    state.bt_updated_ms = 1000;
    // 3000 - 1000 = 2000 ms: exactly the limit, still valid.
    CHECK(core::build_registers(state, 3000, 0).regs[core::reg::kBtC10] == 1000);
    // 3001 - 1000 = 2001 ms: stale. -10 raw is 0xFFF6, status bit 2 is stale.
    const auto regs = core::build_registers(state, 3001, 0).regs;
    CHECK(regs[core::reg::kBtC10] == 0xFFF6);
    CHECK(regs[core::reg::kBtF10] == 0xFFF6);
    CHECK(regs[core::reg::kStatus] == 0x04);
}

TEST_CASE("firmware version packs decimal major and minor into separate bytes") {
    CHECK(core::firmware_version_from("v0.1.0-mvp-3-gabc123") == 0x0001);
    CHECK(core::firmware_version_from("0.2") == 0x0002);
    CHECK(core::firmware_version_from("v12.34.5") == 0x0C22);
    CHECK(core::firmware_version_from("255.256") == 0xFFFF);
    CHECK(core::firmware_version_from("99999999999999999999.1") == 0xFF01);
    CHECK(core::firmware_version_from("abc1234") == 0);
    CHECK(core::firmware_version_from("v1") == 0);
    CHECK(core::firmware_version_from("v1.") == 0);
    CHECK(core::firmware_version_from("v.2") == 0);
    CHECK(core::firmware_version_from(nullptr) == 0);
}
