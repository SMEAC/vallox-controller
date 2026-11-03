#include <Arduino.h>

int counter = 0;

void setup() {
  Serial.begin(115200);
}

void loop() {
  counter++;
  Serial.println("alive " + String(counter));
  delay(500);
  Serial.printf("Flash: %u MB, PSRAM: %u MB\n",
                ESP.getFlashChipSize() / (1024 * 1024),
                ESP.getPsramSize() / (1024 * 1024));
}