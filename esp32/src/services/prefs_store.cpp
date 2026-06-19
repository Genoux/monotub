#include "services/prefs_store.h"
#include "app/config.h"
#include "app/state.h"
#include <Preferences.h>

namespace {
Preferences store;

// v2 stored two thresholds (rh_low/rh_high) plus derived soft offsets.
// v3 stores four explicit band edges. Map the old pair onto the new edges so a
// device keeps the same comfort band it was tuned to.
void migrateV2ToV3() {
  store.begin("monotub", true);
  const bool hasLegacy = store.isKey("rh_low") && store.isKey("rh_high");
  const float legacyLow = store.getFloat("rh_low", DEFAULT_RH_DRY_BELOW);
  const float legacyHigh = store.getFloat("rh_high", DEFAULT_RH_WET_ABOVE);
  store.end();

  if (hasLegacy) {
    g.humiditySettings.rhDryBelow = legacyLow;
    g.humiditySettings.rhBandLow = legacyLow + 1.0F;
    g.humiditySettings.rhBandHigh = legacyHigh - 2.0F;
    g.humiditySettings.rhWetAbove = legacyHigh;
  }
}
}  // namespace

void prefsLoad() {
  store.begin("monotub", true);
  const bool seeded = store.getBool("relay_seeded", false);
  const uint32_t settingsVersion = store.getUInt("set_ver", 0);

  g.fanSettings.ventOnSeconds = store.getUInt("vent_on", DEFAULT_VENT_ON_SECONDS);
  g.fanSettings.ventOffSeconds =
      store.getUInt("vent_off", DEFAULT_VENT_OFF_SECONDS);

  g.humiditySettings.automationEnabled =
      store.getBool("auto", DEFAULT_AUTOMATION_ENABLED);
  g.humiditySettings.rhDryBelow =
      store.getFloat("dry_below", DEFAULT_RH_DRY_BELOW);
  g.humiditySettings.rhBandLow = store.getFloat("band_low", DEFAULT_RH_BAND_LOW);
  g.humiditySettings.rhBandHigh =
      store.getFloat("band_high", DEFAULT_RH_BAND_HIGH);
  g.humiditySettings.rhWetAbove =
      store.getFloat("wet_above", DEFAULT_RH_WET_ABOVE);
  g.humiditySettings.bandHoldSeconds =
      store.getUInt("band_hold", DEFAULT_BAND_HOLD_SECONDS);

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
  g.actuators.dutyFanOn = false;
  g.actuators.fanOn = false;
  g.actuators.mistOn = false;
  store.end();

  if (settingsVersion < SETTINGS_VERSION) {
    if (settingsVersion < 3) migrateV2ToV3();
    // v4: band_hold defaults to 0 (immediate). Reset any persisted non-zero value.
    if (settingsVersion < 4) {
      g.humiditySettings.bandHoldSeconds = DEFAULT_BAND_HOLD_SECONDS;
    }
    // v5: routine FAE removed. on_sec/off_sec/routine keys are orphaned but harmless.
    // v6: conservative 60L polyfill defaults: short fan nudges, longer settle,
    // higher wet edge, and a small band hold to avoid reacting to brief spikes.
    if (settingsVersion < 6) {
      g.fanSettings.ventOnSeconds = DEFAULT_VENT_ON_SECONDS;
      g.fanSettings.ventOffSeconds = DEFAULT_VENT_OFF_SECONDS;
      g.humiditySettings.rhWetAbove = DEFAULT_RH_WET_ABOVE;
      g.humiditySettings.bandHoldSeconds = DEFAULT_BAND_HOLD_SECONDS;
    }
    prefsSave();
  }
}

void prefsSaveRelayState() {
  store.begin("monotub", false);
  store.putBool("relay_fan", g.actuators.fanOn);
  store.putBool("relay_mist", g.actuators.mistOn);
  store.putBool("relay_seeded", true);
  store.end();
}

void prefsSave() {
  store.begin("monotub", false);
  store.putUInt("vent_on", g.fanSettings.ventOnSeconds);
  store.putUInt("vent_off", g.fanSettings.ventOffSeconds);
  store.putBool("auto", g.humiditySettings.automationEnabled);
  store.putFloat("dry_below", g.humiditySettings.rhDryBelow);
  store.putFloat("band_low", g.humiditySettings.rhBandLow);
  store.putFloat("band_high", g.humiditySettings.rhBandHigh);
  store.putFloat("wet_above", g.humiditySettings.rhWetAbove);
  store.putUInt("band_hold", g.humiditySettings.bandHoldSeconds);
  store.putUInt("set_ver", SETTINGS_VERSION);
  store.putBool("fan_manual", g.actuators.manualFanOn);
  store.putBool("mist_manual", g.actuators.manualMistOn);
  store.end();
}
