#include "domain/controller.h"
#include "app/config.h"
#include "app/state.h"
#include "domain/fan_schedule.h"
#include "domain/humidity_control.h"
#include "services/prefs_store.h"

namespace {
bool activeUntil(uint32_t until) {
  return until != 0 && (int32_t)(until - millis()) > 0;
}

float softLowRhPercent() {
  return g.humiditySettings.lowRhPercent + RH_SOFT_LOW_OFFSET_PERCENT;
}

float softHighRhPercent() {
  return g.humiditySettings.highRhPercent - RH_SOFT_HIGH_OFFSET_PERCENT;
}

bool routineAllowedAtRh(float rh) {
  return rh >= softLowRhPercent() && rh <= softHighRhPercent();
}

bool routineShouldPause() {
  if (!g.humiditySettings.automationEnabled) return true;
  return g.environment.ok && !isnan(g.environment.smoothedHumidityPercent) &&
         !routineAllowedAtRh(g.environment.smoothedHumidityPercent);
}

void syncFanRoutinePauseState() {
  const bool paused = routineShouldPause();
  if (paused) {
    if (!g.runtime.fanRoutinePausedByHumidity) {
      if (!g.humiditySettings.automationEnabled) {
        Serial.println("Routine paused: automation off");
      } else {
        Serial.printf("Routine paused: humidity %.1f%% outside %.1f-%.1f%%\n",
                      g.environment.smoothedHumidityPercent, softLowRhPercent(),
                      softHighRhPercent());
      }
    }
    g.runtime.fanRoutinePausedByHumidity = true;
    fanSchedulePause();
    return;
  }

  if (g.runtime.fanRoutinePausedByHumidity) {
    g.runtime.fanRoutinePausedByHumidity = false;
    fanScheduleResumeWithOffWait();
  }
}

bool resolveFanOn(bool correctiveFanOn) {
  if (!g.humiditySettings.automationEnabled) return g.actuators.manualFanOn;
  if (correctiveFanOn) return true;
  if (g.runtime.fanRoutinePausedByHumidity) return false;

  const bool scheduledFanOn =
      g.fanSettings.routineEnabled && g.actuators.scheduledFanOn;
  return scheduledFanOn;
}

void assignFanReason(bool correctiveFanOn) {
  if (!g.humiditySettings.automationEnabled && g.actuators.manualFanOn) {
    g.actuators.fanReason = FanReason::Manual;
  } else if (correctiveFanOn) {
    g.actuators.fanReason = FanReason::HumidityCorrection;
  } else if (resolveFanOn(false)) {
    g.actuators.fanReason = FanReason::ScheduledFae;
  } else {
    g.actuators.fanReason = FanReason::Off;
  }
}

bool resolveMistOn() {
  if (!g.humiditySettings.automationEnabled) return g.actuators.manualMistOn;

  if (!g.environment.ok || isnan(g.environment.smoothedHumidityPercent)) {
    g.actuators.automationMistOn = true;
    return g.actuators.automationMistOn;
  }

  const float rh = g.environment.smoothedHumidityPercent;
  if (rh < g.humiditySettings.lowRhPercent) {
    g.actuators.automationMistOn = true;
  } else if (rh > softHighRhPercent()) {
    g.actuators.automationMistOn = false;
  }

  return g.actuators.automationMistOn;
}
}  // namespace

void controllerInit() {
  g.actuators.scheduledFanOn = false;
  if (g.fanSettings.routineEnabled) {
    g.runtime.nextScheduledFanToggleAt =
        millis() + g.fanSettings.scheduledOnSeconds * 1000UL;
    Serial.printf("Routine init: first ON in %us\n",
                  g.fanSettings.scheduledOnSeconds);
  }
  syncFanRoutinePauseState();
}

ActuatorChanges controllerApplyOutputs() {
  const bool correctiveFanOn = activeUntil(g.runtime.correctiveFanUntil);
  const bool fanOn = resolveFanOn(correctiveFanOn);

  assignFanReason(correctiveFanOn);
  g.actuators.mistReason = g.humiditySettings.automationEnabled
                                ? MistReason::HumidityAutomation
                                : MistReason::Manual;

  return actuatorsApply(fanOn, resolveMistOn());
}

ActuatorChanges controllerForceApplyOutputs() {
  const bool correctiveFanOn = activeUntil(g.runtime.correctiveFanUntil);
  const bool fanOn = resolveFanOn(correctiveFanOn);

  assignFanReason(correctiveFanOn);
  g.actuators.mistReason = g.humiditySettings.automationEnabled
                                ? MistReason::HumidityAutomation
                                : MistReason::Manual;

  return actuatorsForceApply(fanOn, resolveMistOn());
}

ActuatorChanges controllerTick() {
  if (millis() - g.runtime.lastControllerAt <= CONTROLLER_INTERVAL_MS) {
    return {};
  }

  g.runtime.lastControllerAt = millis();
  syncFanRoutinePauseState();
  if (!g.runtime.fanRoutinePausedByHumidity) {
    fanScheduleTick();
  }
  humidityControlTick();
  return controllerApplyOutputs();
}

ActuatorChanges controllerSetRoutineEnabled(bool enabled) {
  if (g.humiditySettings.automationEnabled) {
    if (!g.fanSettings.routineEnabled) {
      g.fanSettings.routineEnabled = true;
      prefsSave();
    }
    syncFanRoutinePauseState();
    return controllerApplyOutputs();
  }

  if (enabled == g.fanSettings.routineEnabled) return {};

  g.fanSettings.routineEnabled = enabled;
  if (!enabled) {
    g.actuators.scheduledFanOn = false;
    g.runtime.fanRoutinePausedByHumidity = false;
  } else {
    syncFanRoutinePauseState();
    if (!g.runtime.fanRoutinePausedByHumidity) {
      fanScheduleBegin();
    }
  }

  prefsSave();
  return controllerApplyOutputs();
}

ActuatorChanges controllerSetAutomationEnabled(bool enabled) {
  if (enabled == g.humiditySettings.automationEnabled) {
    syncFanRoutinePauseState();
    return controllerApplyOutputs();
  }

  g.humiditySettings.automationEnabled = enabled;
  if (enabled) {
    g.fanSettings.routineEnabled = true;
  }
  if (!enabled) {
    g.runtime.correctiveFanUntil = 0;
    g.runtime.highHumiditySince = 0;
    g.runtime.correctiveFanPulseWindowStartedAt = 0;
    g.runtime.correctiveFanPulseCount = 0;
  }
  syncFanRoutinePauseState();
  prefsSave();
  return controllerApplyOutputs();
}

ActuatorChanges controllerSetFanEnabled(bool enabled) {
  if (enabled == g.actuators.manualFanOn) return {};

  g.actuators.manualFanOn = enabled;
  prefsSave();
  return controllerApplyOutputs();
}

ActuatorChanges controllerToggleFan() {
  return controllerSetFanEnabled(!g.actuators.manualFanOn);
}

ActuatorChanges controllerSetMistEnabled(bool enabled) {
  if (enabled == g.actuators.manualMistOn) return {};

  g.actuators.manualMistOn = enabled;
  prefsSave();
  return controllerApplyOutputs();
}

ActuatorChanges controllerToggleMist() {
  return controllerSetMistEnabled(!g.actuators.manualMistOn);
}
