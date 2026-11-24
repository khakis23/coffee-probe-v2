#include "net/modbus_server.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <optional>
#include <span>

#include "core/conn_table.hpp"
#include "core/modbus.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "lwip/sockets.h"

namespace net::modbus {
namespace {

constexpr const char* kTag = "modbus";
constexpr uint16_t kPort = 502;
constexpr int kBacklog = 2;
constexpr int kMaxAcceptsPerService = 2;
constexpr uint32_t kMaxSelectWaitMs = 100;
constexpr uint32_t kRestartIntervalMs = 1000;
constexpr uint32_t kWarnIntervalMs = 10000;

struct RxBuffer {
    std::array<uint8_t, core::modbus::kMaxAduSize> bytes{};
    std::size_t filled{0};
};

int listener_fd = -1;
core::ConnTable clients;
std::array<RxBuffer, core::kMaxConns> rx;
std::array<uint8_t, core::modbus::kMaxAduSize> tx;
core::RegisterBlock registers;
std::optional<uint32_t> last_start_attempt_ms;
std::optional<uint32_t> last_warning_ms;
uint32_t last_response_ms = 0;
std::atomic<uint32_t> loops{0};

void warn(uint32_t now_ms, const char* message, int error = 0) {
    if (!last_warning_ms || now_ms - *last_warning_ms >= kWarnIntervalMs) {
        last_warning_ms = now_ms;
        if (error == 0) {
            ESP_LOGW(kTag, "%s", message);
        } else {
            ESP_LOGW(kTag, "%s (errno %d)", message, error);
        }
    }
}

void close_socket(int fd, uint32_t now_ms) {
    if (lwip_close(fd) < 0) {
        warn(now_ms, "close failed", errno);
    }
}

void disconnect(std::size_t slot, uint32_t now_ms) {
    close_socket(clients.at(slot).fd, now_ms);
    clients.release(slot);
    rx[slot].filled = 0;
    ESP_LOGI(kTag, "Slot %u disconnected", static_cast<unsigned>(slot));
}

bool would_block(int error) { return error == EAGAIN || error == EWOULDBLOCK; }

void listener_failed(uint32_t now_ms, const char* message, int error) {
    warn(now_ms, message, error);
    close_socket(listener_fd, now_ms);
    listener_fd = -1;
    for (std::size_t slot = 0; slot < core::kMaxConns; ++slot) {
        if (clients.at(slot).fd != -1) {
            disconnect(slot, now_ms);
        }
    }
}

esp_err_t start_listener(uint32_t now_ms) {
    if (listener_fd != -1) {
        return ESP_OK;
    }
    last_start_attempt_ms = now_ms;
    const int fd = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        warn(now_ms, "Listener socket creation failed", errno);
        return ESP_FAIL;
    }

    constexpr int enabled = 1;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(kPort);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (lwip_setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) < 0 ||
        lwip_fcntl(fd, F_SETFL, O_NONBLOCK) < 0 ||
        lwip_bind(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0 ||
        lwip_listen(fd, kBacklog) < 0) {
        warn(now_ms, "Listener setup failed", errno);
        close_socket(fd, now_ms);
        return ESP_FAIL;
    }
    listener_fd = fd;
    ESP_LOGI(kTag, "Listening on port %u", static_cast<unsigned>(kPort));
    return ESP_OK;
}

// Only non-blocking mode is essential: without it a recv() or send() could stall the net task.
// The other options improve latency and dead-peer detection, but if one is ever refused the
// client is still served (the idle timeout covers dead peers), so a failure only warns.
// Rejecting every client over an optional option would lock Artisan out entirely.
bool configure_client(int fd, uint32_t now_ms) {
    if (lwip_fcntl(fd, F_SETFL, O_NONBLOCK) != 0) {
        return false;
    }
    constexpr int enabled = 1;
    // A laptop that sleeps or walks away is detected in ~11 s even without the idle timeout.
    constexpr int kKeepIdleSec = 5;
    constexpr int kKeepIntervalSec = 2;
    constexpr int kKeepCount = 3;
    constexpr timeval kSendTimeout{0, 100000};
    const bool options_ok =
        lwip_setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled)) == 0 &&
        lwip_setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enabled, sizeof(enabled)) == 0 &&
        lwip_setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &kKeepIdleSec, sizeof(kKeepIdleSec)) ==
            0 &&
        lwip_setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &kKeepIntervalSec,
                        sizeof(kKeepIntervalSec)) == 0 &&
        lwip_setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &kKeepCount, sizeof(kKeepCount)) == 0 &&
        lwip_setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &kSendTimeout, sizeof(kSendTimeout)) == 0;
    if (!options_ok) {
        warn(now_ms, "Optional client socket option refused", errno);
    }
    return true;
}

void accept_clients(uint32_t now_ms) {
    for (int attempt = 0; attempt < kMaxAcceptsPerService; ++attempt) {
        const int fd = lwip_accept(listener_fd, nullptr, nullptr);
        if (fd < 0) {
            const int error = errno;
            // Only these mean the listener itself is broken. Anything else (ECONNABORTED when
            // lwIP can't allocate for one peer, ENFILE, ENOMEM, ...) concerns that one incoming
            // connection: Artisan's established link must not pay for it.
            if (error == EBADF || error == ENOTSOCK || error == EINVAL) {
                listener_failed(now_ms, "accept failed", error);
            } else if (!would_block(error) && error != EINTR) {
                warn(now_ms, "Incoming connection failed", error);
            }
            return;
        }

        // A peer that connected and then reset or closed before we got to it must not evict a
        // live client: a pending error, or EOF already queued (peek returns 0), means it's gone.
        int pending_error = 0;
        socklen_t length = sizeof(pending_error);
        uint8_t peek_byte = 0;
        if (lwip_getsockopt(fd, SOL_SOCKET, SO_ERROR, &pending_error, &length) != 0 ||
            pending_error != 0 ||
            lwip_recv(fd, &peek_byte, 1, MSG_PEEK | MSG_DONTWAIT) == 0) {
            close_socket(fd, now_ms);
            continue;
        }

        // R4 permits four client sockets outside service(). Accept must succeed before we can
        // evict the oldest, so a fifth exists briefly. Admit and close it before any setup or
        // logging. The default 10 lwIP sockets cover the listener and this transient fifth client.
        int evicted_fd;
        const auto slot = clients.admit(fd, now_ms, &evicted_fd);
        if (evicted_fd != -1) {
            close_socket(evicted_fd, now_ms);
            ESP_LOGI(kTag, "Slot %u evicted", static_cast<unsigned>(slot));
        }
        rx[slot].filled = 0;
        if (!configure_client(fd, now_ms)) {
            warn(now_ms, "Client socket setup failed", errno);
            disconnect(slot, now_ms);
            continue;
        }
        ESP_LOGI(kTag, "Slot %u connected", static_cast<unsigned>(slot));
    }
}

void read_client(std::size_t slot, uint32_t now_ms) {
    const int fd = clients.at(slot).fd;
    auto& buffer = rx[slot];
    const auto received = lwip_recv(fd, buffer.bytes.data() + buffer.filled,
                                    buffer.bytes.size() - buffer.filled, MSG_DONTWAIT);
    if (received <= 0) {
        if (received < 0) {
            const int error = errno;
            if (would_block(error)) {
                return;
            }
            warn(now_ms, "recv failed", error);
        }
        disconnect(slot, now_ms);
        return;
    }

    clients.touch(slot, now_ms);
    buffer.filled += static_cast<std::size_t>(received);
    while (const auto frame_size =
               core::modbus::frame_length(std::span(buffer.bytes).first(buffer.filled))) {
        if (*frame_size > buffer.filled) {
            break;
        }

        const auto reply_size = core::modbus::handle_request(
            std::span(buffer.bytes).first(*frame_size), registers, tx);
        if (reply_size == 0) {
            warn(now_ms, "Malformed Modbus request");
            disconnect(slot, now_ms);
            return;
        }
        const auto sent = lwip_send(fd, tx.data(), reply_size, MSG_DONTWAIT);
        if (sent != static_cast<ssize_t>(reply_size)) {
            warn(now_ms, "Incomplete Modbus response", sent < 0 ? errno : 0);
            disconnect(slot, now_ms);
            return;
        }
        last_response_ms = now_ms;
        buffer.filled -= *frame_size;
        std::memmove(buffer.bytes.data(), buffer.bytes.data() + *frame_size, buffer.filled);
    }

    if (buffer.filled == buffer.bytes.size()) {
        warn(now_ms, "Modbus frame exceeds receive buffer");
        disconnect(slot, now_ms);
    }
}

}  // namespace

esp_err_t start() {
    return start_listener(static_cast<uint32_t>(esp_timer_get_time() / 1000));
}

void service(uint32_t now_ms, uint32_t wait_ms) {
    loops.fetch_add(1, std::memory_order_relaxed);
    if (listener_fd == -1 &&
        (!last_start_attempt_ms || now_ms - *last_start_attempt_ms >= kRestartIntervalMs)) {
        if (start_listener(now_ms) != ESP_OK) {
            return;  // Already logged, retry on a later round.
        }
    }
    if (listener_fd == -1) {
        return;
    }

    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(listener_fd, &readable);
    int max_fd = listener_fd;
    for (std::size_t slot = 0; slot < core::kMaxConns; ++slot) {
        const int fd = clients.at(slot).fd;
        if (fd != -1) {
            FD_SET(fd, &readable);
            max_fd = std::max(fd, max_fd);
        }
    }
    const uint32_t wait = std::min(wait_ms, kMaxSelectWaitMs);
    timeval timeout{0, static_cast<long>(wait * 1000)};
    const int ready = lwip_select(max_fd + 1, &readable, nullptr, nullptr, &timeout);
    if (ready < 0) {
        const int error = errno;
        if (!would_block(error) && error != EINTR) {
            listener_failed(now_ms, "select failed", error);
            return;
        }
    }
    if (ready > 0) {
        // Give established clients their response before processing new arrivals.
        for (std::size_t slot = 0; slot < core::kMaxConns; ++slot) {
            const int fd = clients.at(slot).fd;
            if (fd != -1 && FD_ISSET(fd, &readable)) {
                read_client(slot, now_ms);
            }
        }
    }
    // Expire idle clients after reading (a client whose request just arrived is not idle) and
    // before accepting, so a dead slot is reused instead of evicting a live client like Artisan.
    for (std::size_t slot = 0; slot < core::kMaxConns; ++slot) {
        if (clients.idle_expired(slot, now_ms)) {
            disconnect(slot, now_ms);
        }
    }
    // A broken listener shows up as an accept() error, which restarts it. (No exceptfds:
    // a spurious exception flag with SO_ERROR 0 would otherwise drop Artisan's link.)
    if (ready > 0 && FD_ISSET(listener_fd, &readable)) {
        accept_clients(now_ms);
    }
}

void publish(const core::RegisterBlock& block) { registers = block; }

uint8_t client_count() { return static_cast<uint8_t>(clients.active()); }

uint32_t last_request_ms() { return last_response_ms; }

uint32_t loop_count() { return loops.load(std::memory_order_relaxed); }

}  // namespace net::modbus
