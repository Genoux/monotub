#include "domain/fan_schedule.h"
#include "app/state.h"
#include "services/mqtt_service.h"
#include "services/prefs_store.h"

namespace {
void schedulePhaseTimer(uint32_t durationSec) {
  g.runtime.nextScheduledFanToggleAt = millis() + durationSec * 1000UL;
}

uint32_t currentPhaseSeconds() {
  return g.actuators.scheduledFanOn ? g.fanSettings.scheduledOnSeconds
                                    : g.fanSettings.scheduledOffSeconds;
}

void logSchedule(const char* action) {
  Serial.printf(
      "Routine %s: %s for %us (cycle %us ON / %us OFF)\n",
      action,
      g.actuators.scheduledFanOn ? "ON" : "OFF",
      currentPhaseSeconds(),
      g.fanSettings.scheduledOnSeconds,
      g.fanSettings.scheduledOffSeconds);
}
}  // namespace

void fanScheduleBegin() {
  g.actuators.scheduledFanOn = true;
  schedulePhaseTimer(g.fanSettings.scheduledOnSeconds);
  logSchedule("start");
}

void fanScheduleResumeWithOffWait() {
  g.actuators.scheduledFanOn = false;
  schedulePhaseTimer(g.fanSettings.scheduledOffSeconds);
  logSchedule("resume");
}

void fanSchedulePause() {
  g.actuators.scheduledFanOn = false;
  g.runtime.nextScheduledFanToggleAt = 0;
}

void fanScheduleResyncTimer() {
  if (!g.fanSettings.routineEnabled) return;
  schedulePhaseTimer(currentPhaseSeconds());
  logSchedule("resync");
}

void fanScheduleTick() {
  if (!g.fanSettings.routineEnabled) {
    g.actuators.scheduledFanOn = false;
    return;
  }

  if ((int32_t)(millis() - g.runtime.nextScheduledFanToggleAt) < 0) return;

  g.actuators.scheduledFanOn = !g.actuators.scheduledFanOn;
  uint32_t durationSec = currentPhaseSeconds();
  schedulePhaseTimer(durationSec);
  prefsSaveSchedulePhase();
  logSchedule("phase");
  mqttPublishLog("SCHEDULE %s for %us (on=%us off=%us)",
                 g.actuators.scheduledFanOn ? "ON" : "OFF", durationSec,
                 g.fanSettings.scheduledOnSeconds,
                 g.fanSettings.scheduledOffSeconds);
}
