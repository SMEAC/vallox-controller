#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.printf("Flash: %u MB, PSRAM: %u MB\n",
                ESP.getFlashChipSize() / (1024 * 1024),
                ESP.getPsramSize() / (1024 * 1024));
}

void loop() {
  Serial.println("alive");
  delay(2000);
}