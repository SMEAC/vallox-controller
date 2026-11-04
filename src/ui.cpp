#include "ui.h"
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <stdio.h>

namespace {

const int W = 128;

// Text width in pixels with the currently selected font
int textWidth(Adafruit_GFX& g, const char* s) {
  int16_t x1, y1;
  uint16_t w, h;
  g.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  return w;
}

void drawCentered(Adafruit_GFX& g, const char* s, int baseline, uint16_t colour) {
  int16_t x1, y1;
  uint16_t w, h;
  g.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  g.setTextColor(colour);
  g.setCursor((W - static_cast<int>(w)) / 2 - x1, baseline);
  g.print(s);
}

void hLine(Adafruit_GFX& g, int y) {
  g.drawFastHLine(4, y, W - 8, UI_BLACK);
  g.drawFastHLine(4, y + 1, W - 8, UI_BLACK);
}

// Day of week (0 = Sunday), Sakamoto's method
int weekday(int y, int m, int d) {
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

// "-3.5" from tenths of a degree
void formatTenths(char* out, size_t n, int16_t t10) {
  const bool neg = t10 < 0;
  int v = neg ? -t10 : t10;
  snprintf(out, n, "%s%d.%d", neg ? "-" : "", v / 10, v % 10);
}

// Large temperature "-3.5 C" with a drawn degree sign, centred.
void drawTemperature(Adafruit_GFX& g, bool valid, int16_t t10, int baseline) {
  g.setFont(&FreeSansBold18pt7b);
  if (!valid) {
    drawCentered(g, "--", baseline, UI_BLACK);
    return;
  }
  char num[12];
  formatTenths(num, sizeof(num), t10);
  const int wNum = textWidth(g, num);
  const int wC = textWidth(g, "C");
  const int gap = 4, deg = 12;              // space, then the degree circle
  const int total = wNum + gap + deg + 2 + wC;
  int x = (W - total) / 2;
  g.setTextColor(UI_BLACK);
  g.setCursor(x, baseline);
  g.print(num);
  x += wNum + gap;
  g.drawCircle(x + 5, baseline - 25, 4, UI_BLACK);
  g.drawCircle(x + 5, baseline - 25, 3, UI_BLACK);
  g.setCursor(x + deg + 2, baseline);
  g.print("C");
}

void drawLabel(Adafruit_GFX& g, const char* s, int baseline) {
  g.setFont(&FreeSans9pt7b);
  drawCentered(g, s, baseline, UI_BLACK);
}

}  // namespace

void drawStatusScreen(Adafruit_GFX& g, const ScreenData& d) {
  g.fillScreen(UI_WHITE);
  g.setTextWrap(false);
  char buf[32];

  // ---- time ----
  g.setFont(&FreeSansBold24pt7b);
  if (d.timeValid) snprintf(buf, sizeof(buf), "%02u:%02u", d.hour, d.minute);
  else snprintf(buf, sizeof(buf), "--:--");
  drawCentered(g, buf, 40, UI_BLACK);

  // ---- date ----
  g.setFont(&FreeSans9pt7b);
  if (d.dateValid) {
    static const char* const names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const int wd = weekday(2000 + d.year, d.month, d.day);
    snprintf(buf, sizeof(buf), "%s %02u.%02u.%04u", names[wd], d.day, d.month, 2000u + d.year);
    if (textWidth(g, buf) > W - 4) {
      snprintf(buf, sizeof(buf), "%s %02u.%02u.%02u", names[wd], d.day, d.month, d.year);
    }
  } else {
    snprintf(buf, sizeof(buf), "--.--.----");
  }
  drawCentered(g, buf, 64, UI_BLACK);

  hLine(g, 72);

  // ---- outside ----
  drawLabel(g, "OUTSIDE", 94);
  drawTemperature(g, d.outdoorValid, d.outdoorT10, 130);

  hLine(g, 142);

  // ---- inside ----
  drawLabel(g, "INSIDE", 164);
  drawTemperature(g, d.indoorValid, d.indoorT10, 200);

  g.setFont(&FreeSans12pt7b);
  if (d.rhValid) snprintf(buf, sizeof(buf), "RH %u %%", d.rh);
  else snprintf(buf, sizeof(buf), "RH --");
  drawCentered(g, buf, 228, UI_BLACK);

  if (d.rpmValid) snprintf(buf, sizeof(buf), "%u rpm", d.rpm);
  else snprintf(buf, sizeof(buf), "-- rpm");
  drawCentered(g, buf, 252, UI_BLACK);

  hLine(g, 260);

  // ---- mode ----
  const char* text;
  bool attention = false;               // shown white on black
  if (!d.statusValid) {
    text = "NO DATA";
    attention = true;
  } else if (d.fault) {
    text = "FAULT";
    attention = true;
  } else if (d.unitOff) {
    text = "UNIT OFF";
    attention = true;
  } else {
    text = modeName(d.mode);
    attention = (d.mode == Mode::Boost || d.mode == Mode::Fireplace ||
                 d.mode == Mode::Programmable);
  }
  if (attention) g.fillRect(0, 262, W, 34, UI_BLACK);
  g.setFont(&FreeSansBold18pt7b);
  if (textWidth(g, text) > W - 10) g.setFont(&FreeSansBold12pt7b);
  if (textWidth(g, text) > W - 10) g.setFont(&FreeSans12pt7b);
  drawCentered(g, text, 290, attention ? UI_WHITE : UI_BLACK);
}
