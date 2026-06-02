#include "domain/actuators.h"
#include "app/config.h"
#include "app/state.h"
#include "drivers/relay.h"
#include "services/mqtt_service.h"
#include "services/prefs_store.h"

namespace {
constexpr uint32_t MIN_RELAY_COMMAND_GAP_MS = 800;
constexpr uint32_t RELAY_COMMAND_LOCK_MS = 5000;

bool reconnectSuppressActive() {
  return millis() < g.runtime.relayCommandSuppressUntil;
}

bool relayCommandGapElapsed() {
  return millis() - g.runtime.lastRelayCommandAt >= MIN_RELAY_COMMAND_GAP_MS;
}

bool fanCommandBlocked() {
  return millis() < g.runtime.fanCommandLockUntil || reconnectSuppressActive() ||
         !relayCommandGapElapsed();
}

void lockFanAfterCommand() {
  g.runtime.fanCommandLockUntil = millis() + RELAY_COMMAND_LOCK_MS;
  g.runtime.lastRelayCommandAt = millis();
}

bool setRelayState(uint8_t pin, bool on) {
  if (millis() < g.runtime.relayBootSuppressUntil) return false;
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

  if (fanOn != g.actuators.fanOn && !fanCommandBlocked()) {
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

  if (mistOn != g.actuators.mistOn && !reconnectSuppressActive() &&
      relayCommandGapElapsed()) {
    if (setRelayState(MIST_RELAY_PIN, mistOn)) {
      g.actuators.mistOn = mistOn;
      prefsSaveRelayState();
      Serial.printf("Mist: %s (%s)\n", mistOn ? "ON" : "OFF",
                    mistReasonName(g.actuators.mistReason));
      changes.mistChanged = true;
    }
  }

  return changes;
}

ActuatorChanges actuatorsForceApply(bool fanOn, bool mistOn) {
  relaySet(FAN_RELAY_PIN, fanOn);
  relaySet(MIST_RELAY_PIN, mistOn);

  g.actuators.fanOn = fanOn;
  g.actuators.mistOn = mistOn;
  g.runtime.lastRelayCommandAt = millis();

  digitalWrite(LED_PIN, g.actuators.fanOn ? HIGH : LOW);
  prefsSaveRelayState();

  Serial.printf("Relay sync: fan=%s (%s) mist=%s (%s)\n",
                fanOn ? "ON" : "OFF", fanReasonName(g.actuators.fanReason),
                mistOn ? "ON" : "OFF", mistReasonName(g.actuators.mistReason));

  ActuatorChanges changes;
  changes.fanChanged = true;
  changes.mistChanged = true;
  return changes;
}
