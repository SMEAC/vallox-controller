#pragma once
#include <Arduino.h>
#include <ModbusMaster.h>
#include "mode.h"

// Register addresses exactly as printed in the Vallox manual
// "Modbus Configuration" (D5269). All are 16-bit holding registers.
namespace reg {
constexpr uint16_t EXTRACT_AIR_TEMP    = 4354;   // cK (centikelvin); room air = "inside"
constexpr uint16_t OUTDOOR_AIR_TEMP    = 4356;   // cK
constexpr uint16_t SUPPLY_FAN_RPM      = 4362;   // rpm, "intake motor"
constexpr uint16_t RH_VALUE            = 4363;   // %, 0 = no sensor
constexpr uint16_t BASIC_MODE          = 4609;   // 0 = Home, 1 = Away, 2 = Automatic
constexpr uint16_t POWER               = 4610;   // 0 = on, 5 = off
constexpr uint16_t BOOST_TIMER         = 4612;   // minutes, 0 = not active
constexpr uint16_t CUSTOM_TIMER        = 4613;   // "custom" = fireplace mode
constexpr uint16_t PROG_TIMER          = 4614;   // started by a digital input of the unit
constexpr uint16_t CRITICAL_FAULT      = 4621;   // 1 = critical fault, unit stopped
constexpr uint16_t CLOCK_MINUTE        = 4849;   // 4849..4853 = minute, hour, day, month, year
constexpr uint16_t BOOST_TIMER_ENABLE  = 21766;  // 1 = the timer counts down
constexpr uint16_t CUSTOM_TIMER_ENABLE = 21767;
}  // namespace reg

struct RawSensors {
  uint16_t extractT;    // cK
  uint16_t outdoorT;    // cK
  uint16_t supplyRpm;
  uint16_t rh;          // 0 = no sensor
};

struct RawStatus {
  uint16_t basic;       // 4609
  uint16_t power;       // 4610
  uint16_t boostTimer;  // 4612
  uint16_t customTimer; // 4613
  uint16_t progTimer;   // 4614
  uint16_t fault;       // 4621
};

struct RawClock {
  uint8_t hour, minute, day, month, year;   // year = 0..99
};

// The mode that is really in charge: the timed modes override the basic mode
// (priority as in the manual: programmable > custom/fireplace > boost > basic).
Mode decodeMode(const RawStatus& s);

class Vallox {
 public:
  void begin(Stream& port, uint8_t slaveId) { modbus_.begin(slaveId, port); }

  bool readSensors(RawSensors& out);
  bool readStatus(RawStatus& out);
  bool readClock(RawClock& out);

  // Puts the unit into the given mode. Only registers that really need a new
  // value are written. Returns false if the unit did not accept everything.
  bool setMode(Mode target);

  // The last Modbus operation got no answer at all (unit off / wiring / bus)
  bool busTimedOut() const { return lastError_ == ModbusMaster::ku8MBResponseTimedOut; }
  uint8_t lastError() const { return lastError_; }

 private:
  uint8_t read(uint16_t first, uint8_t count, uint16_t* out);
  bool writeIfDifferent(uint16_t address, uint16_t value);

  ModbusMaster modbus_;
  uint8_t lastError_ = 0;
  bool sensorSingleReads_ = false;
};
