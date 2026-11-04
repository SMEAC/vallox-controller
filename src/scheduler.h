#pragma once
// -----------------------------------------------------------------------------
//  When to look at the ventilation unit.            (pure logic, no Arduino)
//
//  After a mode change (and after power-up) the unit is checked
//      1. every  1 s for the first 20 s          -> Phase::Fast
//      2. every  5 s for the following 5 min     -> Phase::Medium
//      3. once a minute, when the minute of the
//         displayed time changes                 -> Phase::Slow
//
//  A "check" reads the unit; the display is only refreshed if something that
//  is shown has changed (see main.cpp).
//
//  The unit only reports hours and minutes, so the moment the minute rolls
//  over is learned by watching the clock while the checks are frequent
//  (Fast / Medium phase). In the Slow phase the next check is then planned
//  shortly after the predicted rollover. If the unit's clock turns out to be
//  late, the check is repeated every second until the minute has changed; if
//  the prediction is only rough, or 30 minutes old, the check starts a few
//  seconds early and polls until the change shows, which measures it again.
//  If the clock of the unit is set by hand, the rhythm is learned afresh.
// -----------------------------------------------------------------------------
#include <stdint.h>

namespace sched {
// a - b for 32-bit millisecond counters that may wrap around
inline int32_t diff(uint32_t a, uint32_t b) { return static_cast<int32_t>(a - b); }
}  // namespace sched

class MinuteClock {
 public:
  MinuteClock()
      : valid_(false), phaseKnown_(false), fine_(false), probing_(false), retry_(false),
        hour_(0), minute_(0), sinceProbe_(0), lastReadMs_(0), boundaryMs_(0), nextMs_(0) {}

  // Call after every successful read of the unit's clock (hour, minute).
  void observe(uint8_t hour, uint8_t minute, uint32_t now) {
    retry_ = false;
    if (valid_) {
      if (hour != hour_ || minute != minute_) {
        const uint8_t expMinute = static_cast<uint8_t>((minute_ + 1) % 60);
        const uint8_t expHour =
            (minute_ == 59) ? static_cast<uint8_t>((hour_ + 1) % 24) : hour_;
        const bool normal = (minute == expMinute) && (hour == expHour);
        const uint32_t gap = now - lastReadMs_;
        if (!normal) {
          phaseKnown_ = false;                 // clock was set, or minutes were missed
          probing_ = false;
        } else if (gap <= kMaxPhaseGapMs) {
          // rollover happened between two reads that are close together:
          // we know when it was, to within half the gap
          boundaryMs_ = lastReadMs_ + gap / 2 + kMinuteMs;
          phaseKnown_ = true;
          fine_ = (gap <= kFineGapMs);
          if (probing_) {
            probing_ = false;
            sinceProbe_ = 0;
          }
        } else if (probing_) {
          // we started looking early, yet the minute had already changed:
          // the unit's clock runs ahead of our prediction by more than the lead
          phaseKnown_ = false;
          probing_ = false;
        } else if (phaseKnown_) {
          boundaryMs_ += kMinuteMs;            // the expected rollover, seen at the slow cadence
          if (sinceProbe_ < 0xFFFF) ++sinceProbe_;
        }
      } else if (phaseKnown_) {
        // Looking early (probing): the window opens kProbeLeadMs before the predicted
        // rollover. Otherwise: the predicted rollover has passed. Either way the
        // minute has not changed yet, so keep looking.
        const uint32_t from = probing_ ? boundaryMs_ - kProbeLeadMs : boundaryMs_;
        if (sched::diff(now, from) >= 0) retry_ = true;
      }
    }
    hour_ = hour;
    minute_ = minute;
    valid_ = true;
    lastReadMs_ = now;
  }

  // Plan the next Slow-phase check. readOk = the last check could read the clock.
  void scheduleNext(uint32_t now, bool readOk) {
    if (!readOk) {
      nextMs_ = now + kErrorRetryMs;
    } else if (!phaseKnown_) {
      nextMs_ = now + kAcquireMs;              // still learning where the minute starts
    } else if (retry_) {
      nextMs_ = now + kRetryMs;                // waiting for the rollover to show up
    } else {
      // Normal cycle: look just after the predicted rollover. If the phase is only
      // known roughly, or has not been measured for a while, start looking early
      // and poll until the change shows, which measures it again precisely.
      probing_ = !fine_ || sinceProbe_ >= kRephaseEvery;
      uint32_t planned = probing_ ? boundaryMs_ - kProbeLeadMs : boundaryMs_ + kCheckDelayMs;
      if (sched::diff(planned, now) <= 0) planned = now + kRetryMs;
      nextMs_ = planned;
    }
  }

  bool slowDue(uint32_t now) const { return sched::diff(now, nextMs_) >= 0; }
  bool phaseKnown() const { return phaseKnown_; }

  static const uint32_t kMinuteMs = 60000UL;
  static const uint32_t kMaxPhaseGapMs = 5500;   // reads this close give a usable phase
  static const uint32_t kFineGapMs = 1000;       // reads this close give a precise phase
  static const uint32_t kCheckDelayMs = 800;     // look this long after the predicted rollover
  static const uint32_t kProbeLeadMs = 5000;     // re-measure: start looking this early
  static const uint32_t kRetryMs = 1000;
  static const uint32_t kAcquireMs = 2000;
  static const uint32_t kErrorRetryMs = 5000;
  static const uint16_t kRephaseEvery = 30;      // re-measure every 30 minutes

 private:
  bool valid_, phaseKnown_, fine_, probing_, retry_;
  uint8_t hour_, minute_;
  uint16_t sinceProbe_;
  uint32_t lastReadMs_;
  uint32_t boundaryMs_;   // predicted time of the next minute rollover
  uint32_t nextMs_;       // time of the next Slow-phase check
};

class CheckScheduler {
 public:
  enum Phase { Fast, Medium, Slow };

  explicit CheckScheduler(uint32_t fastEveryMs = 1000, uint32_t fastForMs = 20000,
                          uint32_t mediumEveryMs = 5000, uint32_t mediumForMs = 300000)
      : fastEvery_(fastEveryMs), fastFor_(fastForMs), mediumEvery_(mediumEveryMs),
        mediumFor_(mediumForMs), startMs_(0), nextMs_(0) {}

  // Mode change (or power-up): start again with the frequent checks, check now.
  void restart(uint32_t now) {
    startMs_ = now;
    nextMs_ = now;
  }

  Phase phase(uint32_t now) const {
    const uint32_t elapsed = now - startMs_;
    if (elapsed < fastFor_) return Fast;
    if (elapsed < fastFor_ + mediumFor_) return Medium;
    return Slow;
  }

  bool due(uint32_t now, const MinuteClock& clock) const {
    if (phase(now) == Slow) return clock.slowDue(now);
    return sched::diff(now, nextMs_) >= 0;
  }

  // Call when a check has finished. readOk = the clock could be read.
  void done(uint32_t now, MinuteClock& clock, bool readOk) {
    clock.scheduleNext(now, readOk);
    const uint32_t every = (phase(now) == Fast) ? fastEvery_ : mediumEvery_;
    nextMs_ += every;                                  // keep a steady rhythm ...
    if (sched::diff(now, nextMs_) > static_cast<int32_t>(every)) {
      nextMs_ = now;                                   // ... unless we fell behind
    }                                                  // (e.g. during an ePaper refresh)
  }

 private:
  uint32_t fastEvery_, fastFor_, mediumEvery_, mediumFor_;
  uint32_t startMs_;
  uint32_t nextMs_;
};
