# buttons

Polls the four button GPIOs (active low, internal pull-ups) and turns them into menu presses
through `core::Debouncer`. The menu itself is `core::menu` (host-tested). See
[docs/architecture.md](../../docs/architecture.md) → *Code layout*.

- API: `init()`, `poll(now_ms, out)` (every 10 ms from the UI task, the only caller; at most one
  press per button), `ok_down()` (raw OK level for the power-on factory reset in `app_main`).
- A button already down when polling starts is ignored until released (OK still held after the
  power-on factory reset must not open the menu).
- A short press is released within 0.6 s; a long press fires once at 0.8 s (`core/button.hpp`).
  A long OK opens the menu, so a stray tap during a roast never hides the temperature.
- Pins come from `board/board.hpp`, never from literals here. A pin set to `GPIO_NUM_NC` never
  reports a press.
