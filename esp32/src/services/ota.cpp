#include "services/ota.h"
#include "app/config.h"
#include "domain/controller.h"
#include "drivers/relay.h"
#include "secrets.h"
#include <ArduinoOTA.h>
#include <WiFi.h>

static_assert(sizeof(OTA_PASSWORD) > 1, "OTA_PASSWORD must not be empty: ArduinoOTA treats it as no auth");

namespace {
bool otaStarted = false;

void otaStart() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  // handle() blocks for the whole transfer, so the control loop cannot switch
  // the relays meanwhile: drop both first rather than leave mist running blind.
  ArduinoOTA.onStart([] {
    relaySet(FAN_RELAY_PIN, false);
    relaySet(MIST_RELAY_PIN, false);
    Serial.println("OTA: update started, relays off");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA: failed (%u), restoring relays\n", error);
    controllerForceApplyOutputs();
  });
  ArduinoOTA.begin();
  otaStarted = true;
  Serial.printf("OTA: ready as %s.local\n", OTA_HOSTNAME);
}
}  // namespace

// Starts lazily so a board that boots without Wi-Fi still gets OTA once it joins.
void otaHandle() {
  if (!otaStarted) {
    if (!WiFi.isConnected()) return;
    otaStart();
  }
  ArduinoOTA.handle();
}
