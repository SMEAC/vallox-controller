# Vallox MV350 controller (ESP32-S3 + 2.9" black/white ePaper (Waveshare, partial refresh))

Portrait display 128x296: time, date, outside temp, inside temp, RH, fan rpm, current mode.
One button steps through the modes (Home -> Away -> Boost -> Fire(place)); the mode is sent
to the unit 2 s after the last press (`BUTTON_SETTLE_MS`).

All settings (pins, modes, timer minutes, update timing, dead bands) are in `include/config.h`.

## Wiring
ePaper: VCC 3V3, GND, DIN IO11, CLK IO12, CS IO10, DC IO9, RST IO8, BUSY IO7.
Button: IO4 to GND. Check that none of these pins is used by the RTC chip of your board.

## Update behaviour
After a mode change / power-up the unit is read every 1 s for 20 s, then every 5 s for 5 min,
then once per minute right after the minute changes. The ePaper is redrawn only if a shown
value changed. Normal redraws are partial (~0.5 s, no flashing); a full redraw (~2 s, flashing)
is done at start-up, after a mode change, after 60 partial redraws and at least every 12 h.
Attention states (Boost, Fire, External, Fault, Unit off) are shown white on black.

## Host test of the timing logic
    g++ -std=c++11 -Wall -Wextra -o scheduler_test extras/scheduler_test.cpp && ./scheduler_test
