#include "core/register_publish.hpp"

#include <algorithm>

namespace core {
namespace {

bool is_digit(char c) { return c >= '0' && c <= '9'; }

uint16_t read_version_part(const char*& version) {
    uint16_t part = 0;
    while (is_digit(*version)) {
        // Saturate each digit so arbitrarily long numbers cannot overflow.
        part = static_cast<uint16_t>(std::min(255, part * 10 + (*version - '0')));
        ++version;
    }
    return part;
}

}  // namespace

RegisterBlock build_registers(const ProbeState& state, uint32_t now_ms, uint16_t firmware_version) {
    RegisterBlock block;
    auto& regs = block.regs;
    const bool valid = bt_valid(state, now_ms);
    regs[reg::kBtF10] = to_reg(valid ? to_f(state.bt).v : kFaultValue);
    regs[reg::kBtC10] = to_reg(valid ? state.bt.v : kFaultValue);

    uint16_t status = bt_stale(state, now_ms) ? kStale : 0;
    switch (state.fault) {
        case TcFault::None:
            break;
        case TcFault::Open:
            status |= kProbeOpen;
            break;
        case TcFault::ShortGnd:
        case TcFault::ShortVcc:
            status |= kProbeShort;
            break;
        case TcFault::BusError:
            status |= kBusError;
            break;
    }
    if (state.thermal == ThermalLevel::Warn) {
        status |= kOverheatWarn;
    } else if (state.thermal == ThermalLevel::Critical) {
        status |= kOverheatCrit;
    }
    regs[reg::kStatus] = status;
    regs[reg::kDieC10] = state.die ? to_reg(state.die->v) : kNotAvailable;
    regs[reg::kColdJunctionC10] =
        state.cold_junction ? to_reg(state.cold_junction->v) : kNotAvailable;
    regs[reg::kBatteryPct] = kBatteryUnknown;
    regs[reg::kUptimeHi] = static_cast<uint16_t>(state.uptime_s >> 16);
    regs[reg::kUptimeLo] = static_cast<uint16_t>(state.uptime_s);
    regs[reg::kFirmwareVersion] = firmware_version;
    return block;
}

uint16_t firmware_version_from(const char* version) {
    if (version == nullptr) {
        return 0;
    }
    if (*version == 'v') {
        ++version;
    }
    if (!is_digit(*version)) {
        return 0;
    }
    const uint16_t major = read_version_part(version);
    if (*version != '.' || !is_digit(version[1])) {
        return 0;
    }
    ++version;
    const uint16_t minor = read_version_part(version);
    return static_cast<uint16_t>((major << 8) | minor);
}

}  // namespace core
