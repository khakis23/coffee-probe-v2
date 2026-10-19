# Host unit tests for components/core (#7).
#
# This is a standalone plain-CMake project, NOT an ESP-IDF one: it compiles core's sources with
# the host compiler (Apple clang on the Mac, gcc in CI). No IDF include paths are on the search
# path here, so if anything in core #includes an esp_*/freertos/driver header the build fails.
# That is what enforces "core is pure standard C++".
#
#   cmake -S test/host -B build-host && cmake --build build-host && ctest --test-dir build-host --output-on-failure
cmake_minimum_required(VERSION 3.16)
project(coffeeprobe_host_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../components/core)

# Pick up every core source automatically so new files don't need edits here.
# (CONFIGURE_DEPENDS re-globs on build when files are added/removed.)
file(GLOB CORE_SOURCES CONFIGURE_DEPENDS ${CORE_DIR}/*.cpp ${CORE_DIR}/src/*.cpp)
add_library(core STATIC ${CORE_SOURCES})
target_include_directories(core PUBLIC ${CORE_DIR}/include)
target_compile_options(core PRIVATE -Wall -Wextra -Werror)

include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11
    GIT_SHALLOW TRUE
)
# doctest v2.4.11's own CMakeLists asks for an old cmake_minimum_required; CMake 4.x refuses
# that unless told the old policy version is acceptable.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
FetchContent_MakeAvailable(doctest)

# Every test_*.cpp in this directory is part of the test binary.
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp)
add_executable(core_tests ${TEST_SOURCES})
target_link_libraries(core_tests PRIVATE core doctest::doctest_with_main)
target_compile_options(core_tests PRIVATE -Wall -Wextra -Werror)

enable_testing()
add_test(NAME core_tests COMMAND core_tests)
