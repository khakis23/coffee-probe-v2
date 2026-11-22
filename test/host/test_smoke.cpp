// Smoke test: proves the host harness builds and links core with no IDF headers.
// Real tests for core logic start in #8.
#include <doctest/doctest.h>

#include <string_view>

#include "core/core.hpp"

TEST_CASE("core links on the host") {
    CHECK(std::string_view(core::name()) == "core");
}
