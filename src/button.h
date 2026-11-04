#pragma once
#include <Arduino.h>

// One push button to GND. The press is caught by an interrupt, so it is not
// lost while the program is busy with the (slow) ePaper refresh.
class Button {
 public:
  void begin(int pin);
  // true once for every press
  bool consumePress();

 private:
  static void IRAM_ATTR onFalling();
  static volatile bool pressed_;
  static volatile uint32_t lastEdgeMs_;
};
