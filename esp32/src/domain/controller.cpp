#include "domain/controller.h"
#include "app/config.h"
#include "app/state.h"
#include "domain/fan_schedule.h"
#include "domain/humidity_control.h"

namespace {
bool activeUntil(uint32_t until) {
  return until != 0 && (int32_t)(until - millis()) > 0;
}
}  // namespace

void controllerInit() {
  g.actuators.scheduledFanOn = false;
  if (g.fanSettings.routineEnabled) {
    // Use onSeconds as the initial OFF wait so the first scheduled ON
    // happens quickly on boot rather than waiting the full offSeconds.
    g.runtime.nextScheduledFanToggleAt =
        millis() + g.fanSettings.scheduledOnSeconds * 1000UL;
    Serial.printf("Routine init: first ON in %us\n",
                  g.fanSettings.scheduledOnSeconds);
  }
}

ActuatorChanges controllerApplyOutputs() {
  const bool correctiveFanOn = activeUntil(g.runtime.correctiveFanUntil);
  const bool mistPulseOn = activeUntil(g.runtime.mistPulseUntil);
  const bool fanOn = g.actuators.manualFanOn || g.actuators.scheduledFanOn || correctiveFanOn;
  const bool mistOn = g.actuators.manualMistOn || mistPulseOn;

  if (g.actuators.manualFanOn) {
    g.actuators.fanReason = FanReason::Manual;
  } else if (g.actuators.scheduledFanOn) {
    g.actuators.fanReason = FanReason::ScheduledFae;
  } else if (correctiveFanOn) {
    g.actuators.fanReason = FanReason::HumidityCorrection;
  } else {
    g.actuators.fanReason = FanReason::Off;
  }

  if (g.actuators.manualMistOn) {
    g.actuators.mistReason = MistReason::Manual;
  } else if (mistPulseOn) {
    g.actuators.mistReason = MistReason::LowHumidity;
  } else {
    g.actuators.mistReason = MistReason::Off;
  }

  return actuatorsApply(fanOn, mistOn);
}

ActuatorChanges controllerTick() {
  if (millis() - g.runtime.lastControllerAt <= CONTROLLER_INTERVAL_MS) {
    return {};
  }

  g.runtime.lastControllerAt = millis();
  fanScheduleTick();
  humidityControlTick();
  return controllerApplyOutputs();
}
