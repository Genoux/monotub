#include "domain/controller.h"
#include "app/config.h"
#include "app/state.h"
#include "domain/fan_schedule.h"
#include "domain/humidity_control.h"
#include "services/prefs_store.h"

namespace {
bool sensorReadable() {
  return g.environment.ok && !isnan(g.environment.smoothedHumidityPercent);
}

// Debounce the raw band into the acted-upon band: a new band must hold for
// bandHoldSeconds before it engages. Until then the previous action stays put.
HumidityBand updateEngagedBand(float rh) {
  const HumidityBand band = classifyBand(rh);
  if (band != g.runtime.currentBand) {
    g.runtime.currentBand = band;
    g.runtime.bandEnteredAt = millis();
  }
  if (g.runtime.engagedBand == HumidityBand::Unknown ||
      bandHoldRemainingSeconds() == 0) {
    g.runtime.engagedBand = g.runtime.currentBand;
  }
  return g.runtime.engagedBand;
}

void driveFanForBand(HumidityBand band) {
  switch (fanModeForBand(band)) {
    case FanMode::Vent:
      // Start in ON phase — vent immediately when humidity is out of range.
      fanDutyUpdate(true, g.fanSettings.ventOnSeconds,
                    g.fanSettings.ventOffSeconds, true);
      break;
    case FanMode::Off:
      fanDutyUpdate(false, 0, 0);
      break;
  }
}

void driveMistForBand(HumidityBand band) {
  switch (mistIntentForBand(band)) {
    case MistIntent::On:
      g.actuators.automationMistOn = true;
      break;
    case MistIntent::Off:
      g.actuators.automationMistOn = false;
      break;
    case MistIntent::Hold:
      break;
  }
}

void resetBandTracking() {
  g.runtime.currentBand = HumidityBand::Unknown;
  g.runtime.engagedBand = HumidityBand::Unknown;
  g.runtime.bandEnteredAt = 0;
}

// Single linear pass: classify -> debounce -> mist + fan from the band table.
void evaluateAutomation() {
  if (!g.humiditySettings.automationEnabled) {
    fanDutyUpdate(false, 0, 0);
    return;
  }
  if (!sensorReadable()) {
    g.actuators.automationMistOn = true;  // safe fallback: keep humidity up
    fanDutyUpdate(false, 0, 0);
    resetBandTracking();
    return;
  }
  const HumidityBand band = updateEngagedBand(g.environment.smoothedHumidityPercent);
  driveMistForBand(band);
  driveFanForBand(band);
}

bool resolveFanOn() {
  if (!g.humiditySettings.automationEnabled) return g.actuators.manualFanOn;
  return g.actuators.dutyFanOn;
}

bool resolveMistOn() {
  if (!g.humiditySettings.automationEnabled) return g.actuators.manualMistOn;
  return g.actuators.automationMistOn;
}

void assignReasons() {
  if (!g.humiditySettings.automationEnabled) {
    g.actuators.fanReason =
        g.actuators.manualFanOn ? FanReason::Manual : FanReason::Off;
    g.actuators.mistReason = MistReason::Manual;
    return;
  }
  g.actuators.mistReason = MistReason::HumidityAutomation;
  g.actuators.fanReason = g.actuators.dutyFanOn ? FanReason::Vent : FanReason::Off;
}
}  // namespace

void controllerInit() {
  g.actuators.dutyFanOn = false;
  g.runtime.nextDutyToggleAt = 0;
  resetBandTracking();
}

ActuatorChanges controllerApplyOutputs() {
  assignReasons();
  return actuatorsApply(resolveFanOn(), resolveMistOn());
}

ActuatorChanges controllerForceApplyOutputs() {
  assignReasons();
  return actuatorsForceApply(resolveFanOn(), resolveMistOn());
}

ActuatorChanges controllerTick() {
  if (millis() - g.runtime.lastControllerAt <= CONTROLLER_INTERVAL_MS) {
    return {};
  }
  g.runtime.lastControllerAt = millis();
  evaluateAutomation();
  return controllerApplyOutputs();
}

ActuatorChanges controllerSetAutomationEnabled(bool enabled) {
  if (enabled == g.humiditySettings.automationEnabled) {
    return controllerApplyOutputs();
  }

  g.humiditySettings.automationEnabled = enabled;
  if (enabled) {
    resetBandTracking();
  } else {
    fanDutyUpdate(false, 0, 0);
  }
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
