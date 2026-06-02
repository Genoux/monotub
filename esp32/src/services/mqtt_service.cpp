#include "services/mqtt_service.h"
#include "app/config.h"
#include "app/state.h"
#include "domain/controller.h"
#include "domain/fan_schedule.h"
#include "secrets.h"
#include "services/prefs_store.h"
#include "services/telemetry.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <cstdarg>
#include <esp_system.h>

namespace {
WiFiClient netClient;
PubSubClient client(netClient);

bool parseBool(const String& value) {
  return value.equalsIgnoreCase("ON") ||
         value.equalsIgnoreCase("true") ||
         value == "1";
}

bool parsePositiveUInt(const String& value, uint32_t& out) {
  long parsed = value.toInt();
  if (parsed <= 0) return false;
  out = (uint32_t)parsed;
  return true;
}

bool parsePercent(const String& value, float& out) {
  float parsed = value.toFloat();
  if (parsed <= 0.0F || parsed > 100.0F) return false;
  out = parsed;
  return true;
}

void persistAndPublish(bool savePrefs) {
  if (savePrefs) prefsSave();
  ActuatorChanges changes = controllerApplyOutputs();
  mqttPublishActuatorChanges(changes);
  mqttPublishStatus();
}

void onMessage(char* topic, byte* payload, unsigned int length) {
  String msg;
  msg.reserve(length);
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();
  Serial.printf("MQTT %s = %s\n", topic, msg.c_str());

  if (strcmp(topic, T_STATUS_REQ) == 0) {
    mqttPublishFanState();
    mqttPublishMistState();
    mqttPublishStatus();
  } else if (strcmp(topic, T_RELAY_RESYNC) == 0) {
    ActuatorChanges changes = controllerForceApplyOutputs();
    mqttPublishActuatorChanges(changes);
    mqttPublishStatus();
  } else if (strcmp(topic, T_ROUTINE_SET) == 0) {
    ActuatorChanges changes = controllerSetRoutineEnabled(parseBool(msg));
    mqttPublishActuatorChanges(changes);
    mqttPublishStatus();
  } else if (strcmp(topic, T_ON_SEC_SET) == 0) {
    uint32_t value;
    if (!parsePositiveUInt(msg, value) ||
        value == g.fanSettings.scheduledOnSeconds) return;
    g.fanSettings.scheduledOnSeconds = value;
    fanScheduleResyncTimer();
    persistAndPublish(true);
  } else if (strcmp(topic, T_OFF_SEC_SET) == 0) {
    uint32_t value;
    if (!parsePositiveUInt(msg, value) ||
        value == g.fanSettings.scheduledOffSeconds) return;
    g.fanSettings.scheduledOffSeconds = value;
    fanScheduleResyncTimer();
    persistAndPublish(true);
  } else if (strcmp(topic, T_FAN_SET) == 0) {
    ActuatorChanges changes = msg.equalsIgnoreCase("TOGGLE")
                                  ? controllerToggleFan()
                                  : controllerSetFanEnabled(parseBool(msg));
    mqttPublishActuatorChanges(changes);
    mqttPublishStatus();
  } else if (strcmp(topic, T_MIST_SET) == 0) {
    ActuatorChanges changes = msg.equalsIgnoreCase("TOGGLE")
                                  ? controllerToggleMist()
                                  : controllerSetMistEnabled(parseBool(msg));
    mqttPublishActuatorChanges(changes);
    mqttPublishStatus();
  } else if (strcmp(topic, T_AUTOMATION_ENABLED_SET) == 0) {
    ActuatorChanges changes = controllerSetAutomationEnabled(parseBool(msg));
    mqttPublishActuatorChanges(changes);
    mqttPublishStatus();
  } else if (strcmp(topic, T_RH_LOW_SET) == 0) {
    float value;
    if (!parsePercent(msg, value)) return;
    g.humiditySettings.lowRhPercent = value;
    persistAndPublish(true);
  } else if (strcmp(topic, T_RH_HIGH_SET) == 0) {
    float value;
    if (!parsePercent(msg, value)) return;
    g.humiditySettings.highRhPercent = value;
    persistAndPublish(true);
  } else if (strcmp(topic, T_RH_LOW_PERSIST_SET) == 0) {
    uint32_t value;
    if (!parsePositiveUInt(msg, value)) return;
    g.humiditySettings.lowPersistSeconds = value;
    persistAndPublish(true);
  } else if (strcmp(topic, T_RH_HIGH_PERSIST_SET) == 0) {
    uint32_t value;
    if (!parsePositiveUInt(msg, value)) return;
    g.humiditySettings.highPersistSeconds = value;
    persistAndPublish(true);
  } else if (strcmp(topic, T_MIST_PULSE_SET) == 0) {
    uint32_t value;
    if (!parsePositiveUInt(msg, value)) return;
    g.humiditySettings.mistPulseSeconds = value;
    persistAndPublish(true);
  } else if (strcmp(topic, T_FAN_PULSE_SET) == 0) {
    uint32_t value;
    if (!parsePositiveUInt(msg, value)) return;
    g.humiditySettings.fanPulseSeconds = value;
    persistAndPublish(true);
  } else if (strcmp(topic, T_MIST_COOLDOWN_SET) == 0) {
    uint32_t value;
    if (!parsePositiveUInt(msg, value)) return;
    g.humiditySettings.mistCooldownSeconds = value;
    persistAndPublish(true);
  } else if (strcmp(topic, T_FAN_COOLDOWN_SET) == 0) {
    uint32_t value;
    if (!parsePositiveUInt(msg, value)) return;
    g.humiditySettings.fanCooldownSeconds = value;
    persistAndPublish(true);
  } else if (strcmp(topic, T_SIM_ENABLED_SET) == 0) {
    g.environment.simulationEnabled = parseBool(msg);
    if (g.environment.simulationEnabled) {
      g.environment.humidityPercent = g.environment.simulatedHumidityPercent;
      g.environment.smoothedHumidityPercent = g.environment.simulatedHumidityPercent;
      if (isnan(g.environment.temperatureC)) g.environment.temperatureC = 23.0F;
      g.environment.ok = true;
    }
    persistAndPublish(false);
  } else if (strcmp(topic, T_SIM_HUMIDITY_SET) == 0) {
    float value;
    if (!parsePercent(msg, value)) return;
    g.environment.simulatedHumidityPercent = value;
    if (g.environment.simulationEnabled) {
      g.environment.humidityPercent = value;
      g.environment.smoothedHumidityPercent = value;
      if (isnan(g.environment.temperatureC)) g.environment.temperatureC = 23.0F;
      g.environment.ok = true;
    }
    persistAndPublish(false);
  }
}
}  // namespace

namespace {
const char* resetReasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:  return "POWER_ON";
    case ESP_RST_EXT:      return "EXT_PIN";
    case ESP_RST_SW:       return "SOFTWARE";
    case ESP_RST_PANIC:    return "PANIC";
    case ESP_RST_INT_WDT:  return "INT_WATCHDOG";
    case ESP_RST_TASK_WDT: return "TASK_WATCHDOG";
    case ESP_RST_WDT:      return "OTHER_WATCHDOG";
    case ESP_RST_DEEPSLEEP:return "DEEP_SLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO:     return "SDIO";
    default:               return "UNKNOWN";
  }
}
}  // namespace

void mqttPublishLog(const char* fmt, ...) {
  if (!client.connected()) return;
  char msg[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);
  client.publish(T_LOG, msg);
  Serial.printf("[log] %s\n", msg);
}

void mqttInit() {
  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(onMessage);
  client.setBufferSize(1024);
}

bool mqttConnect() {
  if (!WiFi.isConnected()) return false;

  Serial.print("MQTT connect... ");
  bool ok = client.connect(
      MQTT_CLIENT_ID,
      nullptr, nullptr,
      T_CONNECTED, 1, true,
      "offline");
  if (ok) {
    Serial.println("ok");
    g.runtime.relayCommandSuppressUntil = millis() + 1000UL;
    client.publish(T_CONNECTED, "online", true);
    mqttPublishLog("BOOT reason=%s uptime=%lums",
                   resetReasonName(esp_reset_reason()), millis());
    client.subscribe(T_ROUTINE_SET);
    client.subscribe(T_STATUS_REQ);
    client.subscribe(T_RELAY_RESYNC);
    client.subscribe(T_FAN_SET);
    client.subscribe(T_ON_SEC_SET);
    client.subscribe(T_OFF_SEC_SET);
    client.subscribe(T_MIST_SET);
    client.subscribe(T_AUTOMATION_ENABLED_SET);
    client.subscribe(T_RH_LOW_SET);
    client.subscribe(T_RH_HIGH_SET);
    client.subscribe(T_RH_LOW_PERSIST_SET);
    client.subscribe(T_RH_HIGH_PERSIST_SET);
    client.subscribe(T_MIST_PULSE_SET);
    client.subscribe(T_FAN_PULSE_SET);
    client.subscribe(T_MIST_COOLDOWN_SET);
    client.subscribe(T_FAN_COOLDOWN_SET);
    client.subscribe(T_SIM_ENABLED_SET);
    client.subscribe(T_SIM_HUMIDITY_SET);
    mqttPublishFanState();
    mqttPublishMistState();
    mqttPublishStatus();
  } else {
    Serial.printf("failed rc=%d\n", client.state());
  }
  return ok;
}

void mqttMaintain() {
  if (WiFi.isConnected()) {
    if (!client.connected()) {
      if (millis() - g.runtime.lastMqttRetry > MQTT_RETRY_MS) {
        g.runtime.lastMqttRetry = millis();
        mqttConnect();
      }
    } else {
      client.loop();
    }
  } else if (millis() - g.runtime.lastWifiRetry > WIFI_RETRY_MS) {
    g.runtime.lastWifiRetry = millis();
    WiFi.reconnect();
  }
}

void mqttPublishFanState() {
  if (client.connected()) {
    client.publish(T_STATE, g.actuators.fanOn ? "ON" : "OFF", true);
  }
}

void mqttPublishMistState() {
  if (client.connected()) {
    client.publish(T_MIST_STATE, g.actuators.mistOn ? "ON" : "OFF", true);
  }
}

void mqttPublishActuatorChanges(const ActuatorChanges& changes) {
  if (changes.fanChanged) mqttPublishFanState();
  if (changes.mistChanged) mqttPublishMistState();
  if (changes.fanChanged || changes.mistChanged) mqttPublishStatus();
}

void mqttPublishStatus() {
  if (!client.connected()) return;

  JsonDocument doc;
  telemetryWriteStatus(doc);

  char buf[1024];
  size_t n = serializeJson(doc, buf, sizeof(buf));
  if (n >= sizeof(buf)) {
    Serial.println("status JSON truncated; increase buf");
    return;
  }
  client.publish(T_STATUS, (uint8_t*)buf, n, true);
}

void mqttTickStatus() {
  if (millis() - g.runtime.lastStatusAt <= STATUS_INTERVAL_MS) return;
  g.runtime.lastStatusAt = millis();
  mqttPublishStatus();
}
