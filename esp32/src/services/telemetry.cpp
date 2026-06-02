#include "services/telemetry.h"
#include "app/state.h"
#include "domain/humidity_control.h"
#include <WiFi.h>

namespace {
uint32_t remainingSeconds(uint32_t until) {
  if (until == 0 || (int32_t)(until - millis()) <= 0) return 0;
  return (until - millis() + 999UL) / 1000UL;
}

const char* routinePauseReason() {
  if (!g.runtime.fanRoutinePausedByHumidity) return "none";
  if (!g.humiditySettings.automationEnabled) return "automation_off";
  if (!g.environment.ok || isnan(g.environment.smoothedHumidityPercent)) {
    return "unknown";
  }
  const float rh = g.environment.smoothedHumidityPercent;
  const float softLow = g.humiditySettings.lowRhPercent + RH_SOFT_LOW_OFFSET_PERCENT;
  const float softHigh = g.humiditySettings.highRhPercent - RH_SOFT_HIGH_OFFSET_PERCENT;
  if (rh < softLow) return "humidity_low";
  if (rh > softHigh) return "humidity_high";
  return "none";
}
}  // namespace

void telemetryWriteStatus(JsonDocument& doc) {
  doc["on_seconds"] = g.fanSettings.scheduledOnSeconds;
  doc["off_seconds"] = g.fanSettings.scheduledOffSeconds;
  doc["routine"] = g.fanSettings.routineEnabled;
  doc["routine_paused"] = g.runtime.fanRoutinePausedByHumidity;
  doc["routine_pause_reason"] = routinePauseReason();
  doc["fan"] = g.actuators.fanOn;
  doc["manual_fan"] = g.actuators.manualFanOn;
  doc["manual_mist"] = g.actuators.manualMistOn;
  doc["automation_mist"] = g.actuators.automationMistOn;
  doc["mist"] = g.actuators.mistOn;
  doc["uptime_s"] = millis() / 1000;
  doc["rssi"] = WiFi.RSSI();
  doc["ip"] = WiFi.localIP().toString();

  doc["environment_ok"] = g.environment.ok;
  if (g.environment.ok && !isnan(g.environment.humidityPercent) &&
      !isnan(g.environment.temperatureC)) {
    doc["humidity_percent"] = g.environment.humidityPercent;
    doc["temperature_c"] = g.environment.temperatureC;
    doc["smoothed_humidity_percent"] = g.environment.smoothedHumidityPercent;
  }

  doc["automation_enabled"] = g.humiditySettings.automationEnabled;
  doc["simulation_enabled"] = g.environment.simulationEnabled;
  doc["simulated_humidity_percent"] = g.environment.simulatedHumidityPercent;
  doc["fan_reason"] = fanReasonName(g.actuators.fanReason);
  doc["mist_reason"] = mistReasonName(g.actuators.mistReason);
  doc["humidity_low_threshold"] = g.humiditySettings.lowRhPercent;
  doc["humidity_high_threshold"] = g.humiditySettings.highRhPercent;
  doc["humidity_soft_low_threshold"] =
      g.humiditySettings.lowRhPercent + RH_SOFT_LOW_OFFSET_PERCENT;
  doc["humidity_soft_high_threshold"] =
      g.humiditySettings.highRhPercent - RH_SOFT_HIGH_OFFSET_PERCENT;
  doc["humidity_hard_high_threshold"] =
      g.humiditySettings.highRhPercent;
  doc["humidity_low_persist_seconds"] = g.humiditySettings.lowPersistSeconds;
  doc["humidity_high_persist_seconds"] = g.humiditySettings.highPersistSeconds;
  doc["corrective_fan_seconds"] = g.humiditySettings.fanPulseSeconds;
  doc["mist_cooldown_seconds"] = g.humiditySettings.mistCooldownSeconds;
  doc["fan_cooldown_seconds"] = g.humiditySettings.fanCooldownSeconds;
  doc["mist_cooldown_remaining_s"] = humidityMistCooldownRemainingSeconds();
  doc["fan_cooldown_remaining_s"] = humidityFanCooldownRemainingSeconds();
  doc["humidity_high_persist_remaining_s"] =
      humidityHighPersistRemainingSeconds();
  doc["corrective_fan_remaining_s"] =
      remainingSeconds(g.runtime.correctiveFanUntil);
  doc["scheduled_fae_on"] = g.actuators.scheduledFanOn;
  doc["next_scheduled_fae_s"] = remainingSeconds(g.runtime.nextScheduledFanToggleAt);
}
