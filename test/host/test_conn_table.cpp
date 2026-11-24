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
