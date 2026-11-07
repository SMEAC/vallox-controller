#include "display.h"
#include <SPI.h>
#include <GxEPD2_BW.h>
#include "config.h"
#include "ui.h"

namespace {
// SSD1680 waveform table for partial redraws: GxEPD2's table for this panel, except
// for two values from config.h.
//   PARTIAL_DRIVE_REPEATS: repeat count of step 0, which drives only changed pixels.
//     Unchanged pixels are not driven in it, but they still fade slightly over many
//     redraws, and more so the longer this step is.
//   PARTIAL_HOLD_FRAMES: length of step 1, which pushes every pixel toward its new
//     colour (black: BB and WB; white: BW and WW), counteracting that fade.
const uint8_t PARTIAL_LUT[] PROGMEM = {
    0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, PARTIAL_DRIVE_REPEATS,
    PARTIAL_HOLD_FRAMES, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00,
};

// The library's table and loader are private, so load ours first and mark the
// partial mode as active: the library then skips loading its own. The library
// clears that flag after a full refresh or power-off, so ours is reloaded whenever needed.
class TunedPanel : public EPD_DRIVER_CLASS {
 public:
  using EPD_DRIVER_CLASS::EPD_DRIVER_CLASS;
  using EPD_DRIVER_CLASS::refresh;

  void refresh(int16_t x, int16_t y, int16_t w, int16_t h) {
    if (!_using_partial_mode && !_initial_refresh) {
      _writeCommand(0x32);
      _writeDataPGM(PARTIAL_LUT, sizeof(PARTIAL_LUT));
      _using_partial_mode = true;
    }
    EPD_DRIVER_CLASS::refresh(x, y, w, h);
  }
};

// The whole 128 x 296 picture fits in one buffer (4.7 kB).
GxEPD2_BW<TunedPanel, TunedPanel::HEIGHT> panel(
    TunedPanel(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN));
}  // namespace

void EpdDisplay::begin() {
  SPI.begin(EPD_CLK_PIN, -1, EPD_DIN_PIN, EPD_CS_PIN);
  panel.init(0, true, 2, false);
  panel.setRotation(DISPLAY_ROTATION);
}

void EpdDisplay::render(const ScreenData& d, bool full) {
  if (full) panel.setFullWindow();
  else panel.setPartialWindow(0, 0, panel.width(), panel.height());
  panel.firstPage();
  do {
    drawStatusScreen(panel, d);
  } while (panel.nextPage());
  panel.powerOff();     // not hibernate: the panel must keep its image memory for partial updates
}
