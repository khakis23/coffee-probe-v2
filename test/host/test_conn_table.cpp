#include "core/conn_table.hpp"

#include <doctest/doctest.h>

TEST_CASE("fifth connection evicts the oldest even if it received data most recently") {
    core::ConnTable table;
    CHECK(table.active() == 0);
    int evicted = -1;
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(table.admit(10 + static_cast<int>(i), 100 + static_cast<uint32_t>(i), &evicted) == i);
        CHECK(evicted == -1);
    }
    CHECK(table.active() == 4);
    table.touch(0, 200);
    CHECK(table.at(0).last_rx_ms == 200);
    const auto slot = table.admit(20, 201, &evicted);
    CHECK(slot == 0);
    CHECK(evicted == 10);
    CHECK(table.at(slot).fd == 20);
    CHECK(table.at(slot).opened_ms == 201);
    CHECK(table.at(slot).last_rx_ms == 201);
    CHECK(table.active() == 4);
}

TEST_CASE("connection age and idle timeout work across clock rollover") {
    core::ConnTable table;
    int evicted = -1;
    table.admit(10, UINT32_MAX - 99, &evicted);
    table.admit(11, 1, &evicted);
    table.admit(12, 2, &evicted);
    table.admit(13, 3, &evicted);
    CHECK_FALSE(table.idle_expired(0, 9900));  // 100 ms to rollover + 9900 ms.
    CHECK(table.idle_expired(0, 9901));
    CHECK(table.admit(20, 9901, &evicted) == 0);
    CHECK(evicted == 10);
}

TEST_CASE("idle time is measured from last receive and released slots are reused") {
    core::ConnTable table;
    int evicted = -1;
    const auto first = table.admit(10, 0, &evicted);
    const auto second = table.admit(11, 1, &evicted);
    table.touch(first, 200);
    CHECK_FALSE(table.idle_expired(first, 10200));
    CHECK(table.idle_expired(first, 10201));
    table.release(first);
    CHECK(table.at(first).fd == -1);
    CHECK_FALSE(table.idle_expired(first, 10201));
    CHECK(table.active() == 1);
    evicted = 999;
    CHECK(table.admit(12, 10201, &evicted) == first);
    CHECK(evicted == -1);
    CHECK(table.at(second).fd == 11);
    CHECK(table.active() == 2);
}

TEST_CASE("connections admitted in the same millisecond are evicted in admission order") {
    core::ConnTable table;
    int evicted = -1;
    for (int fd = 10; fd < 14; ++fd) {
        table.admit(fd, 500, &evicted);  // Slots 0..3, admitted in that order, same millisecond.
    }
    CHECK(table.admit(20, 500, &evicted) == 0);  // Oldest: fd 10 in slot 0.
    CHECK(evicted == 10);
    // Slot 0 now holds the newest connection. The oldest is fd 11 in slot 1, though every
    // opened_ms is equal and a clock-based tie-break would pick slot 0 again.
    CHECK(table.admit(21, 500, &evicted) == 1);
    CHECK(evicted == 11);
    CHECK(table.admit(22, 500, &evicted) == 2);
    CHECK(evicted == 12);
}
