#include <Arduino.h>
#include "app/state.h"
#include "domain/controller.h"
#include "drivers/relay.h"
#include "drivers/sht31.h"
#include "drivers/wifi_connect.h"
#include "services/mqtt_service.h"
#include "services/prefs_store.h"
#include <esp_system.h>

namespace {
const char* resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:
      return "POWER_ON";
    case ESP_RST_BROWNOUT:
      return "BROWNOUT";
    case ESP_RST_SW:
      return "SOFTWARE";
    case ESP_RST_PANIC:
      return "PANIC";
    case ESP_RST_INT_WDT:
      return "INT_WATCHDOG";
    case ESP_RST_TASK_WDT:
      return "TASK_WATCHDOG";
    case ESP_RST_WDT:
      return "OTHER_WATCHDOG";
    default:
      return "OTHER";
  }
}
}  // namespace

#ifndef RELAY_STRESS_TEST
#define RELAY_STRESS_TEST 0
#endif

#if RELAY_STRESS_TEST
namespace {
constexpr uint32_t STEP_MS = 3000;
constexpr uint32_t BOOT_SETTLE_MS = 3000;

struct RelayStep {
  const char* label;
  bool fanOn;
  bool mistOn;
};

constexpr RelayStep RELAY_STEPS[] = {
    {"all off", false, false},
    {"fan only", true, false},
    {"all off", false, false},
    {"mist only", false, true},
    {"all off", false, false},
    {"fan + mist", true, true},
};

void applyRelayStep(const RelayStep& step) {
  relaySet(FAN_RELAY_PIN, step.fanOn);
  relaySet(MIST_RELAY_PIN, step.mistOn);
  digitalWrite(LED_PIN, step.fanOn || step.mistOn ? HIGH : LOW);
  Serial.printf("Relay stress: %s | fan=%s mist=%s uptime=%lums\n", step.label,
                step.fanOn ? "ON" : "OFF", step.mistOn ? "ON" : "OFF",
                millis());
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  relayInit();
  Serial.println("Relay stress test: no Wi-Fi, MQTT, sensor, prefs, or control loop");
  Serial.printf("Pins: fan=%d mist=%d relay_on_level=%s\n", FAN_RELAY_PIN,
                MIST_RELAY_PIN, RELAY_ON_LEVEL == LOW ? "LOW" : "HIGH");

  applyRelayStep(RELAY_STEPS[0]);
  delay(BOOT_SETTLE_MS);
}

void loop() {
  static size_t stepIndex = 0;

  applyRelayStep(RELAY_STEPS[stepIndex]);
  stepIndex = (stepIndex + 1) % (sizeof(RELAY_STEPS) / sizeof(RELAY_STEPS[0]));
  delay(STEP_MS);
}
#else
void setup() {
  Serial.begin(115200);
  delay(200);

  relayInit();
  sht31Init();
  sht31Read();
  prefsLoad();
  controllerInit();

  Serial.printf("Boot reset: %s\n", resetReasonName(esp_reset_reason()));
  Serial.printf("Relay outputs held OFF for %ums (Wi-Fi init)\n",
                RELAY_BOOT_DELAY_MS);
  g.runtime.relayBootSuppressUntil = millis() + RELAY_BOOT_DELAY_MS;

  wifiInit();
  mqttInit();
}

void loop() {
  mqttMaintain();
  bool environmentChanged = sht31Tick();
  ActuatorChanges actuatorChanges = controllerTick();
  if (millis() >= g.runtime.relayBootSuppressUntil &&
      g.runtime.relayBootSuppressUntil != 0) {
    g.runtime.relayBootSuppressUntil = 0;
    Serial.println("Boot: force-sync relay outputs");
    ActuatorChanges bootChanges = controllerForceApplyOutputs();
    actuatorChanges.fanChanged |= bootChanges.fanChanged;
    actuatorChanges.mistChanged |= bootChanges.mistChanged;
  }
  mqttPublishActuatorChanges(actuatorChanges);
  if (environmentChanged) {
    mqttPublishStatus();
  }
  mqttTickStatus();
}
#endif
