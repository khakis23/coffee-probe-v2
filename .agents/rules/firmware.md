# Firmware code rules

Rules for all C++ in `components/` and `main/`. Rules marked **(guard)** are checked by
`scripts/guards.sh`; the compiler checks the ones marked **(compiler)**.

## F1. Language

- C++ as configured by ESP-IDF (gnu++2b). Host tests compile `core` as standard C++23 without
  extensions (Apple clang on the Mac, gcc in GitHub CI), so `core` must stick to standard C++23 that
  both compilers support. `std::expected` is C++23, which is why the host build is not C++20.
- No exceptions, no RTTI **(compiler)**: no `throw`, `try`, `dynamic_cast`, `typeid`.
- Errors are values: `std::expected<T, Error>` in `core` and our APIs; `esp_err_t` at the IDF boundary,
  converted right there. Never ignore an `esp_err_t`: handle it, log it, or propagate it.
- `ESP_ERROR_CHECK` (which aborts) only for setup steps where continuing is impossible *and* a reboot
  is the right recovery, with a comment saying why. Never in a runtime path.
- Prefer `constexpr`, `enum class`, `std::array`, `std::span`, `std::optional`, fixed-width integers.
- No macros where `constexpr` or a function works. `#pragma once` in headers.

## F2. Where code goes

- **`components/core`**: every piece of logic that can be pure: decoding, filtering, fault model,
  settings validation, menu state machine, Modbus codec, register map, thermal policy, debouncer.
  No IDF, FreeRTOS, driver or `sdkconfig.h` headers **(guard)**. No hidden clocks: pass time in as a
  parameter so tests can control it.
- **Driver components** (`thermocouple`, `display`, `buttons`, `net`, `storage`, `health`): thin
  adapters between hardware/IDF and `core`. If a driver grows logic you'd want to test, move that
  logic into `core`.
- **`main/app_main.cpp`**: wiring only. Init order, task creation, nothing else.
- **`components/bringup`**: temporary diagnostics firmware; delete it when M2 lands, after its decoders
  have moved to `core`.
- Each component keeps a short `README.md`: purpose, public API, which tasks call it.

## F3. Memory and tasks

- No heap allocation in our code **(guard)**: no `new`, `malloc`, `std::vector`, `std::string`,
  `std::function`, `std::map`, `make_unique`/`make_shared`. Use fixed-size buffers sized by named
  constants. IDF components allocating internally during init (Wi-Fi, lwIP) is fine.
- Tasks are created with `xTaskCreateStatic` and statically allocated stacks **(guard)**. Queues,
  mutexes and semaphores use the `...Static` variants too.
- Stack sizes are named constants with a comment on how they were sized. Check
  `uxTaskGetStackHighWaterMark` on hardware and keep ≥ 25 % headroom.
- Shared state goes through `StateStore` (mutex, copy-out snapshot). No other globals shared between
  tasks. Never hold a lock across a blocking call (socket, I2C, SPI, delay).
- Periodic work uses `vTaskDelayUntil`, not `vTaskDelay` in a loop, so timing doesn't drift.
- ISRs only set a flag or post to a queue (`...FromISR`). No logging, no locks, no allocation in ISRs.

## F4. Hardware access

- GPIO numbers only in `board.hpp` **(guard)**; drivers take pins from `board::kBoard.pins`.
- New driver APIs only: `i2c_master`, `spi_master`, `temperature_sensor`, `adc_oneshot`, `esp_lcd`.
  Not the legacy `i2c.h` / `adc.h` drivers (removed in IDF v6).
- Every bus operation has a timeout. Never `portMAX_DELAY` on anything hardware or network related.
- Code must build on IDF v5.3.1 and v6.0.3 until #2 settles the version. Guard version differences
  with `ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(...)` in one place, not scattered.

## F5. Units and values

- Temperatures inside the firmware are integer tenths of a degree (`°C × 10`, `°F × 10`) in `core`
  types, matching the Modbus register map. Convert only at the display/protocol edge.
- A value shown to the user or Artisan carries a timestamp. When stale (> 2 s old) it is reported as a
  fault, never frozen at the last good value.
- Decoders return a value or a typed error. They never guess on a bad frame.

## F6. Style

- `.clang-format` (Google base, 4-space indent, 100 columns). Format changed files before committing.
- Names: `snake_case` functions and variables, `PascalCase` types, `kConstant` constants,
  `namespace` per component (`core::`, `board::`, `net::`, ...).
- Comments explain constraints and reasons the code can't show: units, datasheet quirks, why a
  threshold has its value, which issue a workaround belongs to. Don't restate the code.
- Logging: one `constexpr const char* kTag` per file; `ESP_LOGE` for faults the user would notice,
  `ESP_LOGW` for recovered problems, `ESP_LOGI` sparingly (boot, state changes), never per-sample at
  INFO level.
