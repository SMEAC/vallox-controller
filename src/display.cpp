#include "display.h"
#include <SPI.h>
#include <GxEPD2_BW.h>
#include "config.h"
#include "ui.h"

namespace {
// The whole 128 x 296 picture fits in one buffer (4.7 kB).
GxEPD2_BW<EPD_DRIVER_CLASS, EPD_DRIVER_CLASS::HEIGHT> panel(
    EPD_DRIVER_CLASS(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN));
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
