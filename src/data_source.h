#pragma once
#include "scheduler.h"
#include "screen_data.h"
#include "vallox.h"

// Reads the unit and turns the raw register values into ScreenData.
//  * values are reduced to the resolution that is shown (0.1 degC, 1 %, 10 rpm)
//  * small changes ("dead band") are ignored so that noise does not cause redraws
//  * a failed read keeps the last good value; only after 3 failures in a row
//    a group is shown as "--"
class DataSource {
 public:
  explicit DataSource(Vallox& vallox)
      : vallox_(vallox), sensorFails_(0), statusFails_(0), clockFails_(0) {}

  // Reads sensors, status and clock. Returns true if the clock could be read.
  bool poll(MinuteClock& clock);

  const ScreenData& data() const { return data_; }

 private:
  void applySensors(const RawSensors& s);
  void applyStatus(const RawStatus& s);
  bool applyClock(const RawClock& c);

  Vallox& vallox_;
  ScreenData data_;
  uint8_t sensorFails_, statusFails_, clockFails_;
};
