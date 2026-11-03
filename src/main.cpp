#include <Arduino.h>
#include <ModbusMaster.h>

namespace {
constexpr uint8_t MODBUS_SLAVE_ID = 1;
constexpr int RS485_RX_PIN = 18;
constexpr int RS485_TX_PIN = 17;
constexpr uint32_t MODBUS_BAUD_RATE = 19200;
constexpr uint16_t SUPPLY_TEMPERATURE_REGISTER = 4358;
constexpr uint16_t SUPPLY_FAN_RPM_REGISTER = 4362;
constexpr uint32_t POLL_INTERVAL_MS = 500;

ModbusMaster modbus;
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(MODBUS_BAUD_RATE, SERIAL_8E1, RS485_RX_PIN, RS485_TX_PIN);
  modbus.begin(MODBUS_SLAVE_ID, Serial2);
}

void loop() {
  static uint32_t lastPoll = 0;
  const uint32_t now = millis();
  if (now - lastPoll < POLL_INTERVAL_MS) {
    return;
  }
  lastPoll = now;

  const uint8_t temperatureStatus =
      modbus.readHoldingRegisters(SUPPLY_TEMPERATURE_REGISTER, 1);
  const uint16_t temperatureCentiKelvin =
      temperatureStatus == ModbusMaster::ku8MBSuccess
          ? modbus.getResponseBuffer(0)
          : 0;

  const uint8_t fanStatus =
      modbus.readHoldingRegisters(SUPPLY_FAN_RPM_REGISTER, 1);
  const uint16_t supplyFanRpm =
      fanStatus == ModbusMaster::ku8MBSuccess
          ? modbus.getResponseBuffer(0)
          : 0;

  Serial.print(F("Supply temperature: "));
  if (temperatureStatus == ModbusMaster::ku8MBSuccess) {
    const int32_t temperatureCentiCelsius =
        static_cast<int32_t>(temperatureCentiKelvin) - 27315;
    Serial.print(temperatureCentiCelsius / 100.0f, 2);
    Serial.print(F(" C"));
  } else {
    Serial.printf("read error 0x%02X",
                  static_cast<unsigned int>(temperatureStatus));
  }

  Serial.print(F(" | Supply fan: "));
  if (fanStatus == ModbusMaster::ku8MBSuccess) {
    Serial.printf("%u RPM", static_cast<unsigned int>(supplyFanRpm));
  } else {
    Serial.printf("read error 0x%02X", static_cast<unsigned int>(fanStatus));
  }
  Serial.println();
}