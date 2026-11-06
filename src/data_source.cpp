#include "data_source.h"
#include "config.h"

namespace {

constexpr uint8_t MAX_FAILS = 3;

// centikelvin -> tenths of a degree Celsius, rounded
int16_t centiKelvinToTenths(uint16_t cK) {
  const int32_t centi = static_cast<int32_t>(cK) - 27315;
  return static_cast<int16_t>((centi >= 0 ? centi + 5 : centi - 5) / 10);
}

// range of the temperature registers according to the manual: 21000 .. 33224 cK
bool temperatureOk(uint16_t cK) { return cK >= 21000 && cK <= 33224; }

// Take the new value only if it moved at least `band` away from the old one.
template <typename T>
T withDeadband(bool oldValid, T oldValue, T newValue, T band) {
  if (!oldValid) return newValue;
  const T d = (newValue > oldValue) ? static_cast<T>(newValue - oldValue)
                                    : static_cast<T>(oldValue - newValue);
  return (d >= band) ? newValue : oldValue;
}

// A group of values could not be read. Returns true if it must now be shown as "--".
bool noteFail(const char* group, uint8_t& fails, bool skipped) {
  if (DEBUG_SERIAL && skipped) {
    Serial.printf("[%lu s] %s: not read, the bus is not answering\n", millis() / 1000UL, group);
  }
  if (fails >= MAX_FAILS) return true;
  ++fails;
  if (fails < MAX_FAILS) return false;
  if (DEBUG_SERIAL) {
    Serial.printf("[%lu s] %s: %u failures in a row, now shown as --\n",
                  millis() / 1000UL, group, MAX_FAILS);
  }
  return true;
}

// A group of values was read successfully.
void noteOk(const char* group, uint8_t& fails) {
  if (DEBUG_SERIAL && fails >= MAX_FAILS) {
    Serial.printf("[%lu s] %s: readable again\n", millis() / 1000UL, group);
  }
  fails = 0;
}

}  // namespace

void DataSource::applySensors(const RawSensors& s) {
  if (temperatureOk(s.outdoorT)) {
    data_.outdoorT10 = withDeadband<int16_t>(data_.outdoorValid, data_.outdoorT10,
                                             centiKelvinToTenths(s.outdoorT), TEMP_DEADBAND_T10);
    data_.outdoorValid = true;
  } else {
    data_.outdoorValid = false;
    data_.outdoorT10 = 0;
  }

  if (temperatureOk(s.extractT)) {
    data_.indoorT10 = withDeadband<int16_t>(data_.indoorValid, data_.indoorT10,
                                            centiKelvinToTenths(s.extractT), TEMP_DEADBAND_T10);
    data_.indoorValid = true;
  } else {
    data_.indoorValid = false;
    data_.indoorT10 = 0;
  }

  if (s.rh >= 1 && s.rh <= 100) {                    // 0 = no humidity sensor
    data_.rh = withDeadband<uint8_t>(data_.rhValid, data_.rh,
                                     static_cast<uint8_t>(s.rh), RH_DEADBAND);
    data_.rhValid = true;
  } else {
    data_.rhValid = false;
    data_.rh = 0;
  }

  if (s.supplyRpm <= 10000) {
    const uint16_t rounded = static_cast<uint16_t>(((s.supplyRpm + 5) / 10) * 10);
    data_.rpm = withDeadband<uint16_t>(data_.rpmValid, data_.rpm, rounded, RPM_DEADBAND);
    data_.rpmValid = true;
  } else {
    data_.rpmValid = false;
    data_.rpm = 0;
  }
}

void DataSource::applyStatus(const RawStatus& s) {
  data_.statusValid = true;
  data_.mode = decodeMode(s);
  data_.unitOff = (s.power == 5);
  data_.fault = (s.fault == 1);
}

bool DataSource::applyClock(const RawClock& c) {
  const bool timeOk = c.hour < 24 && c.minute < 60;
  const bool dateOk = c.day >= 1 && c.day <= 31 && c.month >= 1 && c.month <= 12 && c.year < 100;
  data_.timeValid = timeOk;
  data_.hour = timeOk ? c.hour : 0;
  data_.minute = timeOk ? c.minute : 0;
  data_.dateValid = dateOk;
  data_.day = dateOk ? c.day : 0;
  data_.month = dateOk ? c.month : 0;
  data_.year = dateOk ? c.year : 0;
  return timeOk;
}

bool DataSource::poll(MinuteClock& clock) {
  bool busDead = false;         // no answer at all: do not wait for more time-outs

  RawSensors sensors;
  if (vallox_.readSensors(sensors)) {
    noteOk("sensors", sensorFails_);
    applySensors(sensors);
  } else {
    busDead = vallox_.busTimedOut();
    if (noteFail("sensors", sensorFails_, false)) {
      data_.outdoorValid = data_.indoorValid = data_.rhValid = data_.rpmValid = false;
      data_.outdoorT10 = data_.indoorT10 = 0;
      data_.rh = 0;
      data_.rpm = 0;
    }
  }

  RawStatus status;
  const bool statusSkipped = busDead;
  if (!busDead && vallox_.readStatus(status)) {
    noteOk("status", statusFails_);
    applyStatus(status);
  } else {
    if (!busDead) busDead = vallox_.busTimedOut();
    if (noteFail("status", statusFails_, statusSkipped)) {
      data_.statusValid = false;
      data_.mode = Mode::Home;
      data_.unitOff = false;
      data_.fault = false;
    }
  }

  bool clockOk = false;
  RawClock rawClock;
  if (!busDead && vallox_.readClock(rawClock)) {
    clockOk = applyClock(rawClock);
    if (clockOk) {
      clock.observe(rawClock.hour, rawClock.minute, millis());
    } else if (DEBUG_SERIAL) {
      Serial.printf("[%lu s] clock: read returned an invalid time %u:%u\n",
                    millis() / 1000UL, rawClock.hour, rawClock.minute);
    }
  }
  if (clockOk) {
    noteOk("clock", clockFails_);
  } else {
    if (noteFail("clock", clockFails_, busDead)) {
      data_.timeValid = data_.dateValid = false;
      data_.hour = data_.minute = data_.day = data_.month = data_.year = 0;
    }
  }
  return clockOk;
}
