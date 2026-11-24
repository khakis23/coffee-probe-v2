#include "core/conn_table.hpp"

#include <algorithm>

namespace core {

std::size_t ConnTable::admit(int fd, uint32_t now_ms, int* evicted_fd) {
    std::size_t slot = 0;
    for (std::size_t i = 0; i < conns_.size(); ++i) {
        if (conns_[i].fd == -1) {
            slot = i;
            break;
        }
        if (conns_[i].seq < conns_[slot].seq) {
            slot = i;
        }
    }
    *evicted_fd = conns_[slot].fd;
    conns_[slot] = {fd, now_ms, now_ms, next_seq_++};
    return slot;
}

void ConnTable::touch(std::size_t slot, uint32_t now_ms) { conns_[slot].last_rx_ms = now_ms; }

void ConnTable::release(std::size_t slot) { conns_[slot] = {}; }

bool ConnTable::idle_expired(std::size_t slot, uint32_t now_ms) const {
    const auto& conn = at(slot);
    return conn.fd != -1 && now_ms - conn.last_rx_ms > kIdleTimeoutMs;
}

const Conn& ConnTable::at(std::size_t slot) const { return conns_[slot]; }

std::size_t ConnTable::active() const {
    return std::count_if(conns_.begin(), conns_.end(),
                         [](const Conn& conn) { return conn.fd != -1; });
}

}  // namespace core
