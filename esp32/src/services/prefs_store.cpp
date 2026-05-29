#include "services/prefs_store.h"
#include "app/config.h"
#include "app/state.h"
#include <Preferences.h>

namespace {
Preferences store;
}

void prefsLoad() {
  store.begin("monotub", true);
  g.fanSettings.scheduledOnSeconds =
      store.getUInt("on_sec", DEFAULT_ON_SECONDS);
  g.fanSettings.scheduledOffSeconds =
      store.getUInt("off_sec", DEFAULT_OFF_SECONDS);
  g.fanSettings.routineEnabled =
      store.getBool("routine", DEFAULT_ROUTINE_ENABLED);
  g.humiditySettings.automationEnabled =
      store.getBool("auto", DEFAULT_AUTOMATION_ENABLED);
  g.humiditySettings.lowRhPercent =
      store.getFloat("rh_low", DEFAULT_RH_LOW_PERCENT);
  g.humiditySettings.highRhPercent =
      store.getFloat("rh_high", DEFAULT_RH_HIGH_PERCENT);
  g.humiditySettings.lowPersistSeconds =
      store.getUInt("low_persist", DEFAULT_RH_LOW_PERSIST_SECONDS);
  g.humiditySettings.highPersistSeconds =
      store.getUInt("high_persist", DEFAULT_RH_HIGH_PERSIST_SECONDS);
  g.humiditySettings.mistPulseSeconds =
      store.getUInt("mist_pulse", DEFAULT_MIST_PULSE_SECONDS);
  g.humiditySettings.fanPulseSeconds =
      store.getUInt("fan_pulse", DEFAULT_CORRECTIVE_FAN_SECONDS);
  g.humiditySettings.mistCooldownSeconds =
      store.getUInt("mist_cd", DEFAULT_MIST_COOLDOWN_SECONDS);
  g.humiditySettings.fanCooldownSeconds =
      store.getUInt("fan_cd", DEFAULT_FAN_COOLDOWN_SECONDS);
  // Do not restore scheduledFanOn — fan always starts OFF on boot.
  // The routine timer fires naturally and sets it when ready.
  g.actuators.scheduledFanOn = false;
  g.actuators.fanOn  = store.getBool("relay_fan",  DEFAULT_RELAY_FAN_ON);
  g.actuators.mistOn = store.getBool("relay_mist", DEFAULT_RELAY_MIST_ON);
  store.end();
}

void prefsSaveRelayState() {
  store.begin("monotub", false);
  store.putBool("relay_fan",    g.actuators.fanOn);
  store.putBool("relay_mist",   g.actuators.mistOn);
  store.putBool("relay_seeded", true);
  store.end();
}

void prefsLoadSchedulePhase() {
  store.begin("monotub", true);
  g.actuators.scheduledFanOn = store.getBool("sched_fan_on", true);
  store.end();
}

void prefsSaveSchedulePhase() {
  store.begin("monotub", false);
  store.putBool("sched_fan_on", g.actuators.scheduledFanOn);
  store.end();
}

void prefsSave() {
  store.begin("monotub", false);
  store.putUInt("on_sec", g.fanSettings.scheduledOnSeconds);
  store.putUInt("off_sec", g.fanSettings.scheduledOffSeconds);
  store.putBool("routine", g.fanSettings.routineEnabled);
  store.putBool("sched_fan_on", g.actuators.scheduledFanOn);
  store.putBool("auto", g.humiditySettings.automationEnabled);
  store.putFloat("rh_low", g.humiditySettings.lowRhPercent);
  store.putFloat("rh_high", g.humiditySettings.highRhPercent);
  store.putUInt("low_persist", g.humiditySettings.lowPersistSeconds);
  store.putUInt("high_persist", g.humiditySettings.highPersistSeconds);
  store.putUInt("mist_pulse", g.humiditySettings.mistPulseSeconds);
  store.putUInt("fan_pulse", g.humiditySettings.fanPulseSeconds);
  store.putUInt("mist_cd", g.humiditySettings.mistCooldownSeconds);
  store.putUInt("fan_cd", g.humiditySettings.fanCooldownSeconds);
  store.end();
}
