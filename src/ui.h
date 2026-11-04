#pragma once
#include <Adafruit_GFX.h>
#include "screen_data.h"

// Black / white panel.
constexpr uint16_t UI_BLACK = 0x0000;
constexpr uint16_t UI_WHITE = 0xFFFF;

// Draws the whole screen for a 128 x 296 pixel portrait display.
void drawStatusScreen(Adafruit_GFX& g, const ScreenData& d);
