#include <Arduino.h>
#include "app/state.h"
#include "domain/controller.h"
#include "drivers/relay.h"
#include "drivers/sht31.h"
#include "drivers/wifi_connect.h"
#include "services/mqtt_service.h"
#include "services/prefs_store.h"

void setup() {
  Serial.begin(115200);
  delay(200);

  relayInit();
  sht31Init();
  sht31Read();
  prefsLoad();
  Serial.printf("Loaded: on=%us off=%us routine=%s fan=%s mist=%s\n",
                g.fanSettings.scheduledOnSeconds,
                g.fanSettings.scheduledOffSeconds,
                g.fanSettings.routineEnabled ? "ON" : "OFF",
                g.actuators.fanOn ? "ON" : "OFF",
                g.actuators.mistOn ? "ON" : "OFF");

  relaySet(FAN_RELAY_PIN, g.actuators.fanOn);

  // Mist must always be ON after boot.
  if (!g.actuators.mistOn) {
    g.actuators.mistOn = true;
    prefsSaveRelayState();
    Serial.println("Boot: mist forced ON");
  }
  relaySet(MIST_RELAY_PIN, g.actuators.mistOn);

  wifiInit();
  mqttInit();
  controllerInit();
}

void loop() {
  mqttMaintain();
  bool environmentChanged = sht31Tick();
  ActuatorChanges actuatorChanges = controllerTick();
  mqttPublishActuatorChanges(actuatorChanges);
  if (environmentChanged) {
    mqttPublishStatus();
  }
  mqttTickStatus();
}
