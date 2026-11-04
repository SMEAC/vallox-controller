#pragma once
#include <stdint.h>
#include <string.h>
#include "mode.h"

// Everything that is shown on the ePaper, already reduced to the resolution
// that is actually displayed (0.1 degC, 1 %, 10 rpm). Two ScreenData values
// that compare equal produce exactly the same picture, so comparing them is
// how the program decides whether a (slow, flashing) refresh is necessary.
//
// Fields of an invalid group are always zero, so plain comparison works.
struct ScreenData {
  // --- clock of the ventilation unit ------------------------------------
  bool    timeValid;
  uint8_t hour, minute;
  bool    dateValid;
  uint8_t day, month, year;        // year = 0..99 (2000..2099)

  // --- measurements -----------------------------------------------------
  bool    outdoorValid;
  int16_t outdoorT10;              // outdoor air, 1/10 degC
  bool    indoorValid;
  int16_t indoorT10;               // extract air (= room air), 1/10 degC
  bool    rhValid;
  uint8_t rh;                      // relative humidity, %
  bool    rpmValid;
  uint16_t rpm;                    // supply fan ("intake motor"), rounded to 10

  // --- state ------------------------------------------------------------
  bool    statusValid;
  Mode    mode;
  bool    unitOff;                 // register 4610 = 5
  bool    fault;                   // register 4621 = 1 (critical fault)

  ScreenData() { memset(this, 0, sizeof(*this)); }

  bool sameClock(const ScreenData& o) const {
    return timeValid == o.timeValid && hour == o.hour && minute == o.minute &&
           dateValid == o.dateValid && day == o.day && month == o.month &&
           year == o.year;
  }

  bool sameExceptClock(const ScreenData& o) const {
    return outdoorValid == o.outdoorValid && outdoorT10 == o.outdoorT10 &&
           indoorValid == o.indoorValid && indoorT10 == o.indoorT10 &&
           rhValid == o.rhValid && rh == o.rh &&
           rpmValid == o.rpmValid && rpm == o.rpm &&
           statusValid == o.statusValid && mode == o.mode &&
           unitOff == o.unitOff && fault == o.fault;
  }

  bool operator==(const ScreenData& o) const {
    return sameClock(o) && sameExceptClock(o);
  }
  bool operator!=(const ScreenData& o) const { return !(*this == o); }
};
