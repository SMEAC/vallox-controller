// Vallox MV350 controller: ePaper status display + one mode button (Modbus RTU)
#include <Arduino.h>
#include "button.h"
#include <string.h>
#include "config.h"
#include "data_source.h"
#include "display.h"
#include "scheduler.h"
#include "vallox.h"

namespace {

Vallox vallox;
DataSource source(vallox);
EpdDisplay epd;
Button button;
MinuteClock minuteClock;
CheckScheduler scheduler(FAST_CHECK_MS, FAST_PHASE_MS, MEDIUM_CHECK_MS, MEDIUM_PHASE_MS);

ScreenData shown;                 // what the ePaper shows now
bool drawn = false;
uint32_t lastRefreshMs = 0;
uint32_t lastFullMs = 0;
uint16_t partialCount = 0;

bool hasPending = false;          // button pressed, mode not yet sent to the unit
Mode pendingMode = Mode::Home;
uint32_t lastPressMs = 0;

Mode nextMode(Mode base) {
  for (size_t i = 0; i < MODE_CYCLE_LENGTH; ++i) {
    if (MODE_CYCLE[i] == base) return MODE_CYCLE[(i + 1) % MODE_CYCLE_LENGTH];
  }
  return MODE_CYCLE[0];
}

void refresh(const ScreenData& cur, const char* reason, bool forceFull) {
  const uint32_t now = millis();
  const bool full = forceFull || !drawn || partialCount >= PARTIAL_LIMIT ||
                    now - lastFullMs >= MAX_REFRESH_AGE_MS;
  Serial.printf("[%lu s] ePaper %s refresh: %s\n", now / 1000UL, full ? "full" : "partial", reason);
  epd.render(cur, full);
  if (full) { partialCount = 0; lastFullMs = millis(); } else { ++partialCount; }
  shown = cur;
  drawn = true;
  lastRefreshMs = millis();
}

// Reads the unit and redraws the display if (and only if) that is necessary.
void runCheck(bool force, const char* forceReason) {
  const uint32_t now = millis();
  const bool clockOk = source.poll(minuteClock);
  const ScreenData cur = source.data();
  if (!clockOk) Serial.printf("[%lu s] clock not readable (error %u)\n", now / 1000UL, vallox.lastError());

  const char* reason = nullptr;
  if (force) {
    reason = forceReason;
  } else if (!drawn) {
    reason = "start-up";
  } else if (!cur.sameExceptClock(shown)) {
    reason = "values changed";
  } else if (TIME_TRIGGERS_REFRESH && !cur.sameClock(shown)) {
    reason = "minute changed";
  } else if (now - lastRefreshMs >= MAX_REFRESH_AGE_MS) {
    reason = "daily refresh";
  }

  if (reason) refresh(cur, reason, force);
  scheduler.done(millis(), minuteClock, clockOk);
}

void commitMode(Mode target) {
  hasPending = false;
  if (target == source.data().mode) return;       // already there
  Serial.printf("[%lu s] mode -> %s\n", millis() / 1000UL, modeName(target));
  if (!vallox.setMode(target)) {
    Serial.printf("mode change failed (Modbus error %u)\n", vallox.lastError());
  }
  runCheck(true, "mode change");
  scheduler.restart(millis());
}

}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 1500) delay(10);
  Serial.println(F("Vallox controller starting"));

  Serial2.begin(MODBUS_BAUD_RATE, SERIAL_8E1, RS485_RX_PIN, RS485_TX_PIN);
  vallox.begin(Serial2, MODBUS_SLAVE_ID);
  button.begin(BUTTON_PIN);
  epd.begin();

  scheduler.restart(millis());
}

void loop() {
  const uint32_t now = millis();

  if (button.consumePress()) {
    const Mode base = hasPending ? pendingMode : source.data().mode;
    pendingMode = nextMode(base);
    hasPending = true;
    lastPressMs = now;
    Serial.printf("[%lu s] button: %s\n", now / 1000UL, modeName(pendingMode));
  }

  if (hasPending && now - lastPressMs >= BUTTON_SETTLE_MS) {
    commitMode(pendingMode);
    return;
  }

  if (scheduler.due(now, minuteClock)) {
    runCheck(false, nullptr);
  }
  delay(5);
}
