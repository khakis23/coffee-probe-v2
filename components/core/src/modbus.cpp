#include "core/modbus.hpp"

namespace core::modbus {
namespace {

constexpr std::size_t kLengthOffset = 4;
constexpr std::size_t kUnitOffset = 6;
constexpr std::size_t kPduOffset = 7;
constexpr std::size_t kReadRequestSize = kPduOffset + 5;
constexpr std::size_t kReadDataOffset = kPduOffset + 2;
constexpr uint16_t kMaxReadQuantity = 125;

constexpr uint8_t kReadHoldingRegisters = 0x03;
constexpr uint8_t kReadInputRegisters = 0x04;
constexpr uint8_t kIllegalFunction = 0x01;
constexpr uint8_t kIllegalDataAddress = 0x02;
constexpr uint8_t kIllegalDataValue = 0x03;

uint16_t read_be16(std::span<const uint8_t> buf, std::size_t offset) {
    return static_cast<uint16_t>((buf[offset] << 8) | buf[offset + 1]);
}

void write_be16(std::span<uint8_t> buf, std::size_t offset, uint16_t value) {
    buf[offset] = static_cast<uint8_t>(value >> 8);
    buf[offset + 1] = static_cast<uint8_t>(value);
}

void write_header(std::span<const uint8_t> req, std::span<uint8_t> resp, std::size_t size) {
    const uint16_t transaction = read_be16(req, 0);
    const uint8_t unit = req[kUnitOffset];
    write_be16(resp, 0, transaction);
    write_be16(resp, 2, 0);
    write_be16(resp, kLengthOffset, static_cast<uint16_t>(size - kUnitOffset));
    resp[kUnitOffset] = unit;
}

std::size_t write_exception(std::span<const uint8_t> req, std::span<uint8_t> resp, uint8_t code) {
    constexpr std::size_t kExceptionSize = kPduOffset + 2;
    if (resp.size() < kExceptionSize) {
        return 0;
    }
    const auto function = static_cast<uint8_t>(req[kPduOffset] | 0x80);
    write_header(req, resp, kExceptionSize);
    resp[kPduOffset] = function;
    resp[kPduOffset + 1] = code;
    return kExceptionSize;
}

}  // namespace

std::optional<std::size_t> frame_length(std::span<const uint8_t> buf) {
    if (buf.size() < kUnitOffset) {
        return std::nullopt;
    }
    return kUnitOffset + read_be16(buf, kLengthOffset);
}

std::size_t handle_request(std::span<const uint8_t> req, const RegisterBlock& regs,
                           std::span<uint8_t> resp) {
    // A matching frame of at least eight bytes has a unit ID and a function code.
    if (req.size() <= kPduOffset || req.size() > kMaxAduSize || read_be16(req, 2) != 0 ||
        frame_length(req) != req.size()) {
        return 0;
    }

    const uint8_t function = req[kPduOffset];
    if (function != kReadHoldingRegisters && function != kReadInputRegisters) {
        return write_exception(req, resp, kIllegalFunction);
    }
    if (req.size() != kReadRequestSize) {
        return write_exception(req, resp, kIllegalDataValue);
    }

    const std::size_t start = read_be16(req, kPduOffset + 1);
    const std::size_t quantity = read_be16(req, kPduOffset + 3);
    if (quantity == 0 || quantity > kMaxReadQuantity) {
        return write_exception(req, resp, kIllegalDataValue);
    }
    if (start + quantity > reg::kCount) {
        return write_exception(req, resp, kIllegalDataAddress);
    }

    const std::size_t byte_count = 2 * quantity;
    const std::size_t size = kReadDataOffset + byte_count;
    if (resp.size() < size) {
        return 0;
    }
    write_header(req, resp, size);
    resp[kPduOffset] = function;
    resp[kPduOffset + 1] = static_cast<uint8_t>(byte_count);
    for (std::size_t i = 0; i < quantity; ++i) {
        write_be16(resp, kReadDataOffset + 2 * i, regs.regs[start + i]);
    }
    return size;
}

}  // namespace core::modbus
