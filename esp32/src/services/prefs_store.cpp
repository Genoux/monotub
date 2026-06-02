#include "services/prefs_store.h"
#include "app/config.h"
#include "app/state.h"
#include <Preferences.h>

namespace {
Preferences store;
}  // namespace

void prefsLoad() {
  store.begin("monotub", true);
  const bool seeded = store.getBool("relay_seeded", false);
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

  g.actuators.scheduledFanOn = false;
  if (seeded && store.isKey("fan_manual")) {
    g.actuators.manualFanOn = store.getBool("fan_manual", false);
  } else if (seeded) {
    g.actuators.manualFanOn = store.getBool("relay_fan", false);
  } else {
    g.actuators.manualFanOn = false;
  }
  if (seeded && store.isKey("mist_manual")) {
    g.actuators.manualMistOn = store.getBool("mist_manual", true);
  } else if (seeded) {
    g.actuators.manualMistOn = store.getBool("relay_mist", true);
  } else {
    g.actuators.manualMistOn = true;
  }
  g.actuators.automationMistOn = true;
  g.actuators.fanOn = false;
  g.actuators.mistOn = false;
  store.end();
}

void prefsSaveRelayState() {
  store.begin("monotub", false);
  store.putBool("relay_fan", g.actuators.fanOn);
  store.putBool("relay_mist", g.actuators.mistOn);
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
  store.putBool("fan_manual", g.actuators.manualFanOn);
  store.putBool("mist_manual", g.actuators.manualMistOn);
  store.end();
}
