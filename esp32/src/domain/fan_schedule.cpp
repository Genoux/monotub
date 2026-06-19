#include "domain/fan_schedule.h"
#include "app/state.h"
#include "services/mqtt_service.h"

namespace {
void stopCycle() {
  g.actuators.dutyFanOn = false;
  g.runtime.nextDutyToggleAt = 0;
  g.runtime.dutyOnSeconds = 0;
  g.runtime.dutyOffSeconds = 0;
}

void startCycle(uint32_t onSeconds, uint32_t offSeconds, bool startOn) {
  g.runtime.dutyOnSeconds = onSeconds;
  g.runtime.dutyOffSeconds = offSeconds;
  g.actuators.dutyFanOn = startOn;
  const uint32_t phaseSecs = startOn ? onSeconds : offSeconds;
  g.runtime.nextDutyToggleAt = millis() + phaseSecs * 1000UL;
  mqttPublishLog("DUTY start %s for %us (on=%us off=%us)",
                 startOn ? "ON" : "OFF", phaseSecs, onSeconds, offSeconds);
}
}  // namespace

void fanDutyUpdate(bool active, uint32_t onSeconds, uint32_t offSeconds,
                   bool startOn) {
  if (!active || onSeconds == 0 || offSeconds == 0) {
    if (g.runtime.dutyOnSeconds != 0 || g.actuators.dutyFanOn) stopCycle();
    return;
  }

  // Only restart when the cycle was fully stopped (dutyOnSeconds == 0).
  // Parameter changes while running restart immediately.
  const bool wasStopped = g.runtime.dutyOnSeconds == 0;
  const bool paramsChanged = !wasStopped &&
                             (g.runtime.dutyOnSeconds != onSeconds ||
                              g.runtime.dutyOffSeconds != offSeconds);

  if (wasStopped || paramsChanged) {
    // Param changes always start ON for immediate action.
    startCycle(onSeconds, offSeconds, paramsChanged ? true : startOn);
    return;
  }

  if ((int32_t)(millis() - g.runtime.nextDutyToggleAt) < 0) return;

  g.actuators.dutyFanOn = !g.actuators.dutyFanOn;
  const uint32_t phaseSeconds = g.actuators.dutyFanOn ? onSeconds : offSeconds;
  g.runtime.nextDutyToggleAt = millis() + phaseSeconds * 1000UL;
  mqttPublishLog("DUTY %s for %us (on=%us off=%us)",
                 g.actuators.dutyFanOn ? "ON" : "OFF", phaseSeconds, onSeconds,
                 offSeconds);
}
