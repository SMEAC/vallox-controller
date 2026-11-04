#include "vallox.h"
#include "config.h"

Mode decodeMode(const RawStatus& s) {
  if (s.progTimer != 0) return Mode::Programmable;
  if (s.customTimer != 0) return Mode::Fireplace;
  if (s.boostTimer != 0) return Mode::Boost;
  switch (s.basic) {
    case 1:  return Mode::Away;
    case 2:  return Mode::Auto;
    default: return Mode::Home;
  }
}

uint8_t Vallox::read(uint16_t first, uint8_t count, uint16_t* out) {
  const uint8_t result = modbus_.readHoldingRegisters(first, count);
  lastError_ = result;
  if (result == ModbusMaster::ku8MBSuccess) {
    for (uint8_t i = 0; i < count; ++i) out[i] = modbus_.getResponseBuffer(i);
  }
  return result;
}

bool Vallox::readSensors(RawSensors& out) {
  const uint8_t ok = ModbusMaster::ku8MBSuccess;

  if (!sensorSingleReads_) {
    // 4354..4363 in one request: the five temperatures, RH/CO2 level, both fan
    // speeds and the humidity (one bus transaction instead of four).
    uint16_t b[10];
    const uint8_t result = read(reg::EXTRACT_AIR_TEMP, 10, b);
    if (result == ok) {
      out.extractT = b[0];    // 4354
      out.outdoorT = b[2];    // 4356
      out.supplyRpm = b[8];   // 4362
      out.rh = b[9];          // 4363
      return true;
    }
    if (result != ModbusMaster::ku8MBIllegalDataAddress) return false;
    // This unit does not know every register in the block: ask one by one from now on.
    sensorSingleReads_ = true;
    Serial.println(F("Unit rejects the block read 4354-4363, reading single registers instead"));
  }

  if (read(reg::EXTRACT_AIR_TEMP, 1, &out.extractT) != ok) return false;
  if (read(reg::OUTDOOR_AIR_TEMP, 1, &out.outdoorT) != ok) return false;
  if (read(reg::SUPPLY_FAN_RPM, 1, &out.supplyRpm) != ok) return false;
  if (read(reg::RH_VALUE, 1, &out.rh) != ok) {
    if (busTimedOut()) return false;
    out.rh = 0;               // humidity not available in this unit
  }
  return true;
}

bool Vallox::readStatus(RawStatus& out) {
  uint16_t b[6];              // 4609 .. 4614
  if (read(reg::BASIC_MODE, 6, b) != ModbusMaster::ku8MBSuccess) return false;
  out.basic = b[0];           // 4609
  out.power = b[1];           // 4610
  out.boostTimer = b[3];      // 4612
  out.customTimer = b[4];     // 4613
  out.progTimer = b[5];       // 4614

  uint16_t fault = 0;
  if (read(reg::CRITICAL_FAULT, 1, &fault) != ModbusMaster::ku8MBSuccess && busTimedOut()) {
    return false;
  }
  out.fault = fault;
  return true;
}

bool Vallox::readClock(RawClock& out) {
  uint16_t b[5];              // 4849 .. 4853 = minute, hour, day, month, year
  if (read(reg::CLOCK_MINUTE, 5, b) != ModbusMaster::ku8MBSuccess) return false;
  out.minute = static_cast<uint8_t>(b[0]);
  out.hour = static_cast<uint8_t>(b[1]);
  out.day = static_cast<uint8_t>(b[2]);
  out.month = static_cast<uint8_t>(b[3]);
  out.year = static_cast<uint8_t>(b[4]);
  return true;
}

bool Vallox::writeIfDifferent(uint16_t address, uint16_t value) {
  uint16_t current = 0;
  if (read(address, 1, &current) != ModbusMaster::ku8MBSuccess) return false;
  if (current == value) return true;
  lastError_ = modbus_.writeSingleRegister(address, value);
  return lastError_ == ModbusMaster::ku8MBSuccess;
}

bool Vallox::setMode(Mode target) {
  switch (target) {
    case Mode::Home:
    case Mode::Away:
    case Mode::Auto: {
      // Basic mode: first switch off every timed mode, then select the basic mode.
      const uint16_t basic = (target == Mode::Home) ? 0 : (target == Mode::Away) ? 1 : 2;
      return writeIfDifferent(reg::BOOST_TIMER, 0) &&
             writeIfDifferent(reg::CUSTOM_TIMER, 0) &&
             writeIfDifferent(reg::PROG_TIMER, 0) &&
             writeIfDifferent(reg::BASIC_MODE, basic);
    }
    case Mode::Boost:
      // Timed mode: the other timed modes off, timer enabled, then the minutes.
      return writeIfDifferent(reg::CUSTOM_TIMER, 0) &&
             writeIfDifferent(reg::PROG_TIMER, 0) &&
             writeIfDifferent(reg::BOOST_TIMER_ENABLE, 1) &&
             writeIfDifferent(reg::BOOST_TIMER, BOOST_MINUTES);
    case Mode::Fireplace:
      return writeIfDifferent(reg::BOOST_TIMER, 0) &&
             writeIfDifferent(reg::PROG_TIMER, 0) &&
             writeIfDifferent(reg::CUSTOM_TIMER_ENABLE, 1) &&
             writeIfDifferent(reg::CUSTOM_TIMER, FIREPLACE_MINUTES);
    case Mode::Programmable:
      return false;           // only the unit's own digital input starts this one
  }
  return false;
}
