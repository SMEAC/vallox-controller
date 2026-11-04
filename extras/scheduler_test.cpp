// Host-side test of the check schedule (no hardware needed):
//
//     g++ -std=c++11 -Wall -Wextra -o scheduler_test extras/scheduler_test.cpp && ./scheduler_test
//
// A simulated ventilation unit with its own clock is "polled" by the scheduler
// exactly like main.cpp does it, and the check times are verified.
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "../src/scheduler.h"

static int failures = 0;
#define CHECK(cond, ...)                                  \
  do {                                                    \
    if (!(cond)) {                                        \
      ++failures;                                         \
      std::printf("  FAIL: ");                            \
      std::printf(__VA_ARGS__);                           \
      std::printf("\n");                                  \
    }                                                     \
  } while (0)

struct Check {
  uint32_t t;                    // ms since start of the run
  CheckScheduler::Phase phase;
};

// The unit's clock: minute = floor(((t * (1 + drift)) + offset) / 60 s) + jump
struct FakeUnit {
  double offsetMs;               // how far into its minute the unit is at t = 0
  double driftPpm;               // + = unit clock runs fast, - = slow
  long jumpMinutes;              // clock set by the user during the run
  uint32_t jumpAtMs;
  void read(uint32_t t, uint8_t& h, uint8_t& m) const {
    double u = t * (1.0 + driftPpm * 1e-6) + offsetMs;
    long minutes = static_cast<long>(u / 60000.0);
    if (jumpMinutes != 0 && t >= jumpAtMs) minutes += jumpMinutes;
    m = static_cast<uint8_t>(minutes % 60);
    h = static_cast<uint8_t>((minutes / 60) % 24);
  }
  // time (ms since start) of the unit's n-th minute rollover, found by search
  uint32_t trueRolloverAtOrAfter(uint32_t t) const {
    uint8_t h0, m0, h1, m1;
    read(t, h0, m0);
    for (uint32_t x = t + 1;; ++x) {
      read(x, h1, m1);
      if (h1 != h0 || m1 != m0) return x;
    }
  }
};

// Run the schedule like main.cpp: poll whenever due; a poll takes pollMs,
// an "ePaper refresh" (every refreshEveryMin-th check in Slow) takes refreshMs.
static std::vector<Check> run(const FakeUnit& unit, uint32_t base, uint32_t durationMs,
                              uint32_t restartAtMs = 0xFFFFFFFF) {
  CheckScheduler sched;
  MinuteClock clock;
  std::vector<Check> checks;
  sched.restart(base);
  bool restarted = false;
  for (uint32_t t = 0; t < durationMs; ++t) {
    const uint32_t now = base + t;
    if (!restarted && t >= restartAtMs) {   // button pressed -> mode change
      sched.restart(now);
      restarted = true;
    }
    if (sched.due(now, clock)) {
      uint8_t h, m;
      unit.read(t, h, m);
      clock.observe(h, m, now);
      Check c = {t, sched.phase(now)};
      checks.push_back(c);
      sched.done(now, clock, true);
    }
  }
  return checks;
}

static int count(const std::vector<Check>& c, CheckScheduler::Phase p, uint32_t from, uint32_t to) {
  int n = 0;
  for (size_t i = 0; i < c.size(); ++i)
    if (c[i].phase == p && c[i].t >= from && c[i].t < to) ++n;
  return n;
}

// Longest time from a real minute change of the unit to the first Slow-phase
// check at or after it (looking at rollovers between fromMs and toMs).
static int worstDelayAfterRollover(const FakeUnit& unit, const std::vector<Check>& c,
                                   uint32_t fromMs, uint32_t toMs) {
  int worst = 0;
  uint32_t roll = unit.trueRolloverAtOrAfter(fromMs);
  while (roll + 10000 < toMs) {
    int delay = -1;
    for (size_t i = 0; i < c.size(); ++i) {
      if (c[i].phase == CheckScheduler::Slow && c[i].t >= roll) {
        delay = static_cast<int>(c[i].t - roll);
        break;
      }
    }
    if (delay < 0) return 1 << 30;
    if (delay > worst) worst = delay;
    roll = unit.trueRolloverAtOrAfter(roll);
  }
  return worst;
}

static void testRhythm(const char* name, double offsetMs, double driftPpm, uint32_t base,
                       uint32_t minutes, int maxDelayMs = 2000) {
  std::printf("%s\n", name);
  FakeUnit unit = {offsetMs, driftPpm, 0, 0};
  const uint32_t dur = minutes * 60000UL;
  std::vector<Check> c = run(unit, base, dur);

  // Phase 1: every second for 20 s  (t = 0 .. 19 s)
  CHECK(count(c, CheckScheduler::Fast, 0, 20000) == 20, "fast phase: %d checks, expected 20",
        count(c, CheckScheduler::Fast, 0, 20000));
  for (size_t i = 1; i < 20 && i < c.size(); ++i)
    CHECK(c[i].t - c[i - 1].t == 1000, "fast spacing %u ms", c[i].t - c[i - 1].t);

  // Phase 2: every 5 s for 5 min  (t = 20 s .. 320 s)
  CHECK(count(c, CheckScheduler::Medium, 20000, 320000) == 60,
        "medium phase: %d checks, expected 60", count(c, CheckScheduler::Medium, 20000, 320000));
  for (size_t i = 21; i < 79 && i < c.size(); ++i)
    CHECK(c[i].t - c[i - 1].t == 5000, "medium spacing %u ms at t=%u", c[i].t - c[i - 1].t, c[i].t);

  // Phase 3: the first check after every real minute change must follow quickly,
  // and there must not be many more checks than one per minute.
  int slow = count(c, CheckScheduler::Slow, 0, 1u << 30);
  int worst = worstDelayAfterRollover(unit, c, 8 * 60000UL, dur);
  int expectedSlow = static_cast<int>(minutes) - 5;           // minutes after the first 5:20
  // one check per minute, plus a short burst of early probes (about 6 reads) once at the
  // start of the slow phase and again every 30 minutes
  int allowedSlow = expectedSlow + 8 + 8 * static_cast<int>(minutes / 30);
  CHECK(slow >= expectedSlow - 1 && slow <= allowedSlow,
        "slow phase: %d checks, expected %d..%d", slow, expectedSlow, allowedSlow);
  CHECK(worst <= maxDelayMs, "a check came %d ms after the real minute change", worst);
  std::printf("  fast=%d medium=%d slow=%d (about %d minutes), worst delay after rollover %d ms\n",
              count(c, CheckScheduler::Fast, 0, 1u << 30), count(c, CheckScheduler::Medium, 0, 1u << 30),
              slow, expectedSlow, worst);
}

int main() {
  testRhythm("unit clock 0.0 s into its minute", 0.0, 0.0, 1000, 20);
  testRhythm("unit clock 17.3 s into its minute", 17300.0, 0.0, 1000, 20);
  testRhythm("unit clock 59.9 s into its minute", 59900.0, 0.0, 1000, 20);
  testRhythm("unit clock runs 2000 ppm slow (7 s per hour)", 30000.0, -2000.0, 1000, 90);
  testRhythm("unit clock runs 300 ppm fast (26 s per day)", 30000.0, +300.0, 1000, 120);
  testRhythm("unit clock runs 2000 ppm fast (absurdly fast; error limited by the 30 min re-measure)",
             30000.0, +2000.0, 1000, 90, 5000);
  testRhythm("millis() wraps around during the run", 12345.0, 0.0, 0xFFFFFFFFUL - 400000UL, 20);

  // Button press in the middle of the Slow phase restarts the frequent checks
  {
    std::printf("mode change at 12 min restarts fast/medium phases\n");
    FakeUnit unit = {20000.0, 0.0, 0, 0};
    std::vector<Check> c = run(unit, 5000, 30 * 60000UL, 12 * 60000UL);
    const uint32_t T = 12 * 60000UL;
    CHECK(count(c, CheckScheduler::Fast, T, T + 20000) == 20, "after mode change: fast=%d",
          count(c, CheckScheduler::Fast, T, T + 20000));
    CHECK(count(c, CheckScheduler::Medium, T + 20000, T + 320000) == 60, "after mode change: medium=%d",
          count(c, CheckScheduler::Medium, T + 20000, T + 320000));
    int slowAfter = count(c, CheckScheduler::Slow, T + 320000, 30 * 60000UL);
    CHECK(slowAfter >= 12 && slowAfter <= 24, "slow checks after restart: %d", slowAfter);
    std::printf("  after restart: fast=%d medium=%d slow=%d\n",
                count(c, CheckScheduler::Fast, T, T + 20000),
                count(c, CheckScheduler::Medium, T + 20000, T + 320000), slowAfter);
  }

  // Somebody sets the unit's clock forward by 7 minutes at t = 40 min:
  // the schedule must find the new rhythm again within a couple of minutes.
  {
    std::printf("unit clock set +7 min at 40 min\n");
    FakeUnit unit = {9000.0, 0.0, 7, 40 * 60000UL};
    std::vector<Check> c = run(unit, 1000, 60 * 60000UL);
    int worstAfter = worstDelayAfterRollover(unit, c, 43 * 60000UL, 60 * 60000UL);
    CHECK(worstAfter <= 2000, "after the clock was set, checks are %d ms late", worstAfter);
    std::printf("  worst delay after rollover, from minute 43: %d ms\n", worstAfter);
  }

  std::printf(failures ? "\n%d FAILURE(S)\n" : "\nall schedule tests passed\n", failures);
  return failures ? 1 : 0;
}
