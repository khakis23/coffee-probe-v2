# board

The board profile: a Kconfig choice (`PROBE_BOARD_XIAO_C3` / `PROBE_BOARD_GENERIC_C3`) and
`include/board/board.hpp`, a `constexpr` pin map + Wi-Fi TX power for the selected board. This is
the **only** place GPIO numbers may appear; drivers take their pins from `board::kBoard`. Created
by #4. New boards add a Kconfig entry, a `#elif` block in `board.hpp`, and an
`sdkconfig.defaults.<board>` file at the repo root (see [docs/dev-setup.md](../../docs/dev-setup.md)).
