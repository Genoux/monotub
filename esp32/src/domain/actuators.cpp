#include "domain/actuators.h"
#include "app/config.h"
#include "app/state.h"
#include "drivers/relay.h"
#include "services/mqtt_service.h"
#include "services/prefs_store.h"

namespace {
constexpr uint32_t MIN_RELAY_COMMAND_GAP_MS = 800;
constexpr uint32_t SCHEDULE_RELAY_COMMAND_LOCK_MS = 5000;

bool reconnectSuppressActive() {
  return millis() < g.runtime.relayCommandSuppressUntil;
}

bool relayCommandGapElapsed() {
  return millis() - g.runtime.lastRelayCommandAt >= MIN_RELAY_COMMAND_GAP_MS;
}

bool scheduleFanCommandBlocked() {
  return millis() < g.runtime.fanCommandLockUntil || reconnectSuppressActive() ||
         !relayCommandGapElapsed();
}

bool scheduleMistCommandBlocked() {
  return millis() < g.runtime.mistCommandLockUntil || reconnectSuppressActive() ||
         !relayCommandGapElapsed();
}

void lockFanAfterCommand() {
  g.runtime.fanCommandLockUntil = millis() + SCHEDULE_RELAY_COMMAND_LOCK_MS;
  g.runtime.lastRelayCommandAt = millis();
}

void lockMistAfterCommand() {
  g.runtime.mistCommandLockUntil = millis() + SCHEDULE_RELAY_COMMAND_LOCK_MS;
  g.runtime.lastRelayCommandAt = millis();
}

bool setRelayState(uint8_t pin, bool on) {
  if (reconnectSuppressActive() || !relayCommandGapElapsed()) return false;
  relaySet(pin, on);
  g.runtime.lastRelayCommandAt = millis();
  mqttPublishLog("RELAY pin=%d state=%s uptime=%lums", pin, on ? "ON" : "OFF",
                 millis());
  return true;
}
}  // namespace

ActuatorChanges actuatorsApply(bool fanOn, bool mistOn) {
  ActuatorChanges changes;

  // Manual hold: user toggled via x/m — routine must not overwrite it.
  if (g.runtime.manualFanHold) {
    fanOn = g.actuators.fanOn;
  }
  if (g.runtime.manualMistHold) {
    mistOn = g.actuators.mistOn;
  }

  if (fanOn != g.actuators.fanOn && !scheduleFanCommandBlocked()) {
    if (setRelayState(FAN_RELAY_PIN, fanOn)) {
      lockFanAfterCommand();
      g.actuators.fanOn = fanOn;
      prefsSaveRelayState();
      Serial.printf("Fan: %s (%s)\n", fanOn ? "ON" : "OFF",
                    fanReasonName(g.actuators.fanReason));
      changes.fanChanged = true;
    }
  }
  digitalWrite(LED_PIN, g.actuators.fanOn ? HIGH : LOW);

  if (mistOn != g.actuators.mistOn && !scheduleMistCommandBlocked()) {
    if (setRelayState(MIST_RELAY_PIN, mistOn)) {
      lockMistAfterCommand();
      g.actuators.mistOn = mistOn;
      prefsSaveRelayState();
      Serial.printf("Mist: %s (%s)\n", mistOn ? "ON" : "OFF",
                    mistReasonName(g.actuators.mistReason));
      changes.mistChanged = true;
    }
  }

  return changes;
}

ActuatorChanges actuatorsToggleManualFan() {
  ActuatorChanges changes;

  const bool fanOn = !g.actuators.fanOn;
  if (!setRelayState(FAN_RELAY_PIN, fanOn)) return changes;

  lockFanAfterCommand();
  g.runtime.manualFanHold = true;
  g.actuators.fanOn = fanOn;
  g.actuators.manualFanOn = g.actuators.fanOn;
  g.actuators.fanReason =
      g.actuators.fanOn ? FanReason::Manual : FanReason::Off;
  prefsSaveRelayState();
  digitalWrite(LED_PIN, g.actuators.fanOn ? HIGH : LOW);
  Serial.printf("Fan toggle: %s (manual)\n", g.actuators.fanOn ? "ON" : "OFF");
  changes.fanChanged = true;
  return changes;
}

ActuatorChanges actuatorsToggleManualMist() {
  ActuatorChanges changes;

  const bool mistOn = !g.actuators.mistOn;
  if (!setRelayState(MIST_RELAY_PIN, mistOn)) return changes;

  lockMistAfterCommand();
  g.runtime.manualMistHold = true;
  g.actuators.mistOn = mistOn;
  g.actuators.manualMistOn = g.actuators.mistOn;
  g.actuators.mistReason =
      g.actuators.mistOn ? MistReason::Manual : MistReason::Off;
  prefsSaveRelayState();
  Serial.printf("Mist toggle: %s (manual)\n", g.actuators.mistOn ? "ON" : "OFF");
  changes.mistChanged = true;
  return changes;
}

void actuatorsClearManualFanHold() {
  g.runtime.manualFanHold = false;
  g.actuators.manualFanOn = false;
}

void actuatorsClearManualMistHold() {
  g.runtime.manualMistHold = false;
  g.actuators.manualMistOn = false;
}

void actuatorsSetManualFan(bool on) {
  g.actuators.manualFanOn = on;
}

void actuatorsSetManualMist(bool on) {
  g.actuators.manualMistOn = on;
}
