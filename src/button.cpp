#include "button.h"

namespace {
constexpr uint32_t LOCKOUT_MS = 250;   // contact bounce / double presses
}

volatile bool Button::pressed_ = false;
volatile uint32_t Button::lastEdgeMs_ = 0;

void IRAM_ATTR Button::onFalling() {
  const uint32_t now = millis();
  if (now - lastEdgeMs_ >= LOCKOUT_MS) {
    pressed_ = true;
    lastEdgeMs_ = now;
  }
}

void Button::begin(int pin) {
  pinMode(pin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pin), onFalling, FALLING);
}

bool Button::consumePress() {
  if (!pressed_) return false;
  pressed_ = false;
  return true;
}
