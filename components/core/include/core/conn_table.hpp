#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace core {

inline constexpr std::size_t kMaxConns = 4;
inline constexpr uint32_t kIdleTimeoutMs = 10000;

struct Conn {
    int fd{-1};
    uint32_t opened_ms{0};
    uint32_t last_rx_ms{0};
    // Admission order. The clock can't order two connections admitted in the same millisecond.
    // 64-bit so it never wraps (2^64 admissions), which keeps "oldest = smallest" simply true.
    uint64_t seq{0};
};

class ConnTable {
public:
    // Returns the first free slot or replaces the oldest (earliest admitted) connection.
    // The caller closes evicted_fd (-1 when a free slot was used).
    std::size_t admit(int fd, uint32_t now_ms, int* evicted_fd);
    void touch(std::size_t slot, uint32_t now_ms);
    void release(std::size_t slot);
    bool idle_expired(std::size_t slot, uint32_t now_ms) const;
    const Conn& at(std::size_t slot) const;
    std::size_t active() const;

private:
    std::array<Conn, kMaxConns> conns_{};
    uint64_t next_seq_{0};
};

}  // namespace core
