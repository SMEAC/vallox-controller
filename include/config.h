#pragma once
#include <Arduino.h>
#include "../src/mode.h"

// =============================================================================
//  Vallox MV350 controller - settings
//  Everything you are likely to want to change is in this one file.
// =============================================================================

// ---- Modbus RTU (unchanged from the first test) -----------------------------
constexpr uint8_t  MODBUS_SLAVE_ID  = 1;
constexpr int      RS485_RX_PIN     = 18;
constexpr int      RS485_TX_PIN     = 17;
constexpr uint32_t MODBUS_BAUD_RATE = 19200;          // serial format 8E1

// ---- Debugging ---------------------------------------------------------------
// true = also log every failed Modbus read on the USB serial port (register,
// error), reads skipped because the bus is silent, and groups of values that
// switch to "--" or recover. The normal messages are always printed.
constexpr bool DEBUG_SERIAL = true;

// ---- ePaper: Waveshare 2.9", 296x128, black/white (supports partial refresh) -
// Wiring (module pin -> ESP32-S3-RS485-CAN header):
//   VCC -> 3V3   GND -> GND   DIN -> IO11   CLK -> IO12
//   CS  -> IO10  DC  -> IO9   RST -> IO8    BUSY -> IO7
// Check on your board that none of these pins is used by the RTC chip.
constexpr int EPD_CS_PIN   = 10;
constexpr int EPD_DIN_PIN  = 11;    // SPI MOSI
constexpr int EPD_CLK_PIN  = 12;    // SPI SCK
constexpr int EPD_DC_PIN   = 9;
constexpr int EPD_RST_PIN  = 8;
constexpr int EPD_BUSY_PIN = 7;

// GxEPD2 driver class of the panel.
//   GxEPD2_290_BS : Waveshare 2.9" V2 (SSD1680 controller) - default
//   GxEPD2_290    : Waveshare 2.9" V1 (IL3820 controller, older modules)
// If the display stays blank or shows noise, try the other one.
#define EPD_DRIVER_CLASS GxEPD2_290_BS

// 0 = connector at the bottom edge, 2 = turned by 180 degrees.
constexpr uint8_t DISPLAY_ROTATION = 0;

// ---- The one button ---------------------------------------------------------
// Connect a push button between this pin and GND (internal pull-up is used).
constexpr int      BUTTON_PIN        = 4;
// After the last press, wait this long before the mode is really changed, so
// that you can press several times to reach a mode without waiting for the
// slow ePaper after every press. 0 = react immediately.
constexpr uint32_t BUTTON_SETTLE_MS  = 2000;

// Modes the button steps through, in this order (then it starts again).
// Possible: Home, Away, Auto, Boost, Fireplace
constexpr Mode MODE_CYCLE[] = {Mode::Home, Mode::Away, Mode::Boost, Mode::Fireplace};
constexpr size_t MODE_CYCLE_LENGTH = sizeof(MODE_CYCLE) / sizeof(MODE_CYCLE[0]);

// Duration of the timed modes started with the button.
constexpr uint16_t BOOST_MINUTES     = 30;
constexpr uint16_t FIREPLACE_MINUTES = 15;

// ---- When to look at the unit (after a mode change / power-up) --------------
constexpr uint32_t FAST_CHECK_MS    = 1000;      // every 1 s ...
constexpr uint32_t FAST_PHASE_MS    = 20000;     // ... for the first 20 s
constexpr uint32_t MEDIUM_CHECK_MS  = 5000;      // then every 5 s ...
constexpr uint32_t MEDIUM_PHASE_MS  = 300000;    // ... for 5 minutes
//                                               // then once per minute, when the minute changes

// ---- When to redraw the ePaper ------------------------------------------------
// A redraw happens only when something shown has changed. Normal redraws are
// "partial" (about 0.5 s, no flashing). A full redraw (about 2 s, panel flashes,
// removes ghosting) is done at start-up, after a mode change, after
// PARTIAL_LIMIT partial redraws in a row and at least every MAX_REFRESH_AGE_MS.
constexpr bool     TIME_TRIGGERS_REFRESH = true;      // clock follows the minutes
constexpr uint16_t PARTIAL_LIMIT         = 60;
constexpr uint32_t MAX_REFRESH_AGE_MS    = 12UL * 3600UL * 1000UL;

// A measurement only changes the display if it moved at least this much since
// the value that is shown (avoids needless redraws because of 0.1 degC / 10 rpm noise).
constexpr int16_t  TEMP_DEADBAND_T10 = 2;        // 0.2 degC
constexpr uint16_t RPM_DEADBAND      = 30;       // rpm
constexpr uint8_t  RH_DEADBAND       = 2;        // % relative humidity
