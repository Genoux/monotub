#include "services/telemetry.h"
#include "app/state.h"
#include "domain/humidity_control.h"
#include <WiFi.h>

namespace {
uint32_t remainingSeconds(uint32_t until) {
  if (until == 0 || (int32_t)(until - millis()) <= 0) return 0;
  return (until - millis() + 999UL) / 1000UL;
}
}  // namespace

void telemetryWriteStatus(JsonDocument& doc) {
  doc["on_seconds"] = g.fanSettings.scheduledOnSeconds;
  doc["off_seconds"] = g.fanSettings.scheduledOffSeconds;
  doc["routine"] = g.fanSettings.routineEnabled;
  doc["fan"] = g.actuators.fanOn;
  doc["manual_fan"] = g.actuators.manualFanOn;
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
  doc["fan_reason"] = fanReasonName(g.actuators.fanReason);
  doc["mist_reason"] = mistReasonName(g.actuators.mistReason);
  doc["humidity_low_threshold"] = g.humiditySettings.lowRhPercent;
  doc["humidity_high_threshold"] = g.humiditySettings.highRhPercent;
  doc["humidity_low_persist_seconds"] = g.humiditySettings.lowPersistSeconds;
  doc["humidity_high_persist_seconds"] = g.humiditySettings.highPersistSeconds;
  doc["mist_pulse_seconds"] = g.humiditySettings.mistPulseSeconds;
  doc["corrective_fan_seconds"] = g.humiditySettings.fanPulseSeconds;
  doc["mist_cooldown_seconds"] = g.humiditySettings.mistCooldownSeconds;
  doc["fan_cooldown_seconds"] = g.humiditySettings.fanCooldownSeconds;
  doc["mist_cooldown_remaining_s"] = humidityMistCooldownRemainingSeconds();
  doc["fan_cooldown_remaining_s"] = humidityFanCooldownRemainingSeconds();
  doc["mist_pulse_remaining_s"] = remainingSeconds(g.runtime.mistPulseUntil);
  doc["corrective_fan_remaining_s"] =
      remainingSeconds(g.runtime.correctiveFanUntil);
  doc["scheduled_fae_on"] = g.actuators.scheduledFanOn;
  doc["next_scheduled_fae_s"] = remainingSeconds(g.runtime.nextScheduledFanToggleAt);
}
