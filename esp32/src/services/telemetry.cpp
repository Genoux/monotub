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
  doc["fan"] = g.actuators.fanOn;
  doc["mist"] = g.actuators.mistOn;
  doc["manual_fan"] = g.actuators.manualFanOn;
  doc["manual_mist"] = g.actuators.manualMistOn;
  doc["automation_mist"] = g.actuators.automationMistOn;
  doc["fan_reason"] = fanReasonName(g.actuators.fanReason);
  doc["mist_reason"] = mistReasonName(g.actuators.mistReason);
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

  // Band model — the whole control law is described by these fields.
  doc["rh_dry_below"] = g.humiditySettings.rhDryBelow;
  doc["rh_band_low"] = g.humiditySettings.rhBandLow;
  doc["rh_band_high"] = g.humiditySettings.rhBandHigh;
  doc["rh_wet_above"] = g.humiditySettings.rhWetAbove;
  doc["band_hold_seconds"] = g.humiditySettings.bandHoldSeconds;
  doc["current_band"] = humidityBandName(g.runtime.engagedBand);
  doc["band_hold_remaining_s"] = bandHoldRemainingSeconds();

  // Fan cycle.
  doc["vent_on_seconds"] = g.fanSettings.ventOnSeconds;
  doc["vent_off_seconds"] = g.fanSettings.ventOffSeconds;
  doc["duty_fan_on"] = g.actuators.dutyFanOn;
  doc["next_duty_toggle_s"] = remainingSeconds(g.runtime.nextDutyToggleAt);
}
