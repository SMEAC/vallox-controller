# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Firmware for an ESP32-S3 (Waveshare ESP32-S3-RS485-CAN board, `env:rs485can`) that talks Modbus RTU over RS485 to a Vallox MV350 ventilation unit. It shows time, date, temperatures, RH, fan rpm and mode on a 2.9" black/white ePaper (128x296 portrait, GxEPD2) and lets one push button cycle the operating mode. See [README.md](README.md) for wiring and the user-facing update behaviour.

Reference material (Vallox Modbus manual D5269, register map spreadsheet) lives outside this repo in `c:\Users\gavin\Nextcloud\Private\Projects\Vallox-Controller\Docs\`. Register addresses in [src/vallox.h](src/vallox.h) are written exactly as printed in that manual.

## Commands

```sh
pio run                          # build
pio run -t upload                # flash
pio device monitor               # serial log, 115200 baud (USB CDC)

# Host-side test of the timing logic (no hardware; the only test there is)
g++ -std=c++11 -Wall -Wextra -o scheduler_test extras/scheduler_test.cpp && ./scheduler_test
```

`test/` is the empty PlatformIO template; there are no `pio test` tests.

## Architecture

Data flows in one direction each check: `Vallox` (raw Modbus registers) → `DataSource` (decoded, rounded, dead-banded `ScreenData`) → `main.cpp` compares with what is shown → `EpdDisplay`/`ui.cpp` redraws only if needed.

- **[include/config.h](include/config.h)** — all tunables (pins, Modbus settings, `MODE_CYCLE`, timer minutes, check timing, refresh limits, dead bands). Put new settings here, not in `.cpp` files.
- **[src/vallox.*](src/vallox.cpp)** — Modbus access. Reads registers in blocks; if the unit rejects the 4354–4363 block with *illegal data address* it permanently falls back to single-register reads. `setMode()` only writes registers whose value differs (read-before-write) and clears other timed modes before setting the target. `decodeMode()` encodes the priority programmable > fireplace (custom timer) > boost > basic mode. `Mode::Programmable` is display-only and never set.
- **[src/data_source.*](src/data_source.cpp)** — converts centikelvin → 0.1 °C, validates ranges, applies dead bands, keeps last good values until 3 consecutive read failures, and feeds the clock into `MinuteClock`.
- **[src/screen_data.h](src/screen_data.h)** — the invariant that matters: two equal `ScreenData` values produce an identical picture. Invalid groups must keep their fields zeroed so plain comparison works; the struct is `memset` to zero. When adding a displayed field, add it to `sameClock`/`sameExceptClock` and reduce it to displayed resolution in `DataSource`.
- **[src/scheduler.h](src/scheduler.h)** — header-only, pure logic (no Arduino includes) so it compiles on the host for `extras/scheduler_test.cpp`; keep it that way. `CheckScheduler` handles the Fast (1 s) / Medium (5 s) / Slow phases after power-up or a mode change; in the Slow phase `MinuteClock` predicts when the unit's minute rolls over (the unit only reports h:m) and schedules a check just after it, re-measuring by probing early. All time arithmetic must be wrap-safe via `sched::diff`.
- **[src/main.cpp](src/main.cpp)** — loop: button presses accumulate a pending mode that is committed `BUTTON_SETTLE_MS` after the last press; `refresh()` decides partial vs. full ePaper refresh (full at start-up, mode change, after `PARTIAL_LIMIT` partials, or `MAX_REFRESH_AGE_MS`).
- **[src/display.cpp](src/display.cpp) / [src/ui.cpp](src/ui.cpp)** — GxEPD2 panel ownership vs. drawing on a generic `Adafruit_GFX`. After a refresh the panel is `powerOff()`'d, not hibernated, because partial updates need the controller's image memory. Panel driver class is selected by `EPD_DRIVER_CLASS` in config.h. [layout_preview_bw.png](layout_preview_bw.png) shows the intended layout.
- **[src/button.*](src/button.cpp)** — ISR-based so presses are not lost during the blocking ePaper refresh.

`lib/ModbusMaster` is a vendored copy of the ModbusMaster library; other libraries come from `lib_deps` in [platformio.ini](platformio.ini).
