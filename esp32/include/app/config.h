#pragma once

#include <Arduino.h>
#include <cstdint>

// KEYES/Songle relay module: active LOW inputs.
// Relay outputs are held at the requested state; this is not a latching module.
#define FAN_RELAY_PIN  18
#define MIST_RELAY_PIN 25
#define LED_PIN        2

constexpr uint8_t RELAY_ON_LEVEL = LOW;
constexpr uint8_t RELAY_OFF_LEVEL = HIGH;

// SHT31 I2C
#define SHT31_SDA_PIN  21
#define SHT31_SCL_PIN  22
#define SHT31_I2C_ADDR 0x44

// Relay NVS defaults — assume OFF (physical relay is de-energised on unknown state).
constexpr bool DEFAULT_RELAY_FAN_ON  = false;
constexpr bool DEFAULT_RELAY_MIST_ON = false;

// Fan routine defaults
constexpr uint32_t DEFAULT_ON_SECONDS = 30;
constexpr uint32_t DEFAULT_OFF_SECONDS = 1200;
constexpr bool DEFAULT_ROUTINE_ENABLED = true;

// Humidity automation defaults. Automation adds moisture when dry and vents when wet.
constexpr bool DEFAULT_AUTOMATION_ENABLED = true;
constexpr float DEFAULT_RH_LOW_PERCENT = 85.0F;
constexpr float DEFAULT_RH_HIGH_PERCENT = 90.0F;
constexpr float RH_SOFT_LOW_OFFSET_PERCENT = 1.0F;
constexpr float RH_SOFT_HIGH_OFFSET_PERCENT = 2.0F;
constexpr uint32_t DEFAULT_RH_LOW_PERSIST_SECONDS = 180;
constexpr uint32_t DEFAULT_RH_HIGH_PERSIST_SECONDS = 180;
constexpr uint32_t DEFAULT_MIST_PULSE_SECONDS = 15;
constexpr uint32_t DEFAULT_CORRECTIVE_FAN_SECONDS = 15;
constexpr uint32_t DEFAULT_MIST_COOLDOWN_SECONDS = 600;
constexpr uint32_t DEFAULT_FAN_COOLDOWN_SECONDS = 180;

// MQTT topics
#define T_LOG         "monotub/log"
#define T_STATE       "monotub/fan/state"
#define T_MIST_STATE  "monotub/mist/state"
#define T_STATUS      "monotub/status"
#define T_STATUS_REQ  "monotub/status/request"
#define T_RELAY_RESYNC "monotub/relay/resync"
#define T_CONNECTED   "monotub/connected"
#define T_ROUTINE_SET "monotub/routine/set"
#define T_FAN_SET     "monotub/fan/set"
#define T_ON_SEC_SET  "monotub/fan/on_seconds/set"
#define T_OFF_SEC_SET "monotub/fan/off_seconds/set"
#define T_MIST_SET    "monotub/mist/set"

#define T_AUTOMATION_ENABLED_SET "monotub/automation/enabled/set"
#define T_RH_LOW_SET             "monotub/humidity/low_threshold/set"
#define T_RH_HIGH_SET            "monotub/humidity/high_threshold/set"
#define T_RH_LOW_PERSIST_SET     "monotub/humidity/low_persist_seconds/set"
#define T_RH_HIGH_PERSIST_SET    "monotub/humidity/high_persist_seconds/set"
#define T_MIST_PULSE_SET         "monotub/mist/pulse_seconds/set"
#define T_FAN_PULSE_SET          "monotub/fan/corrective_seconds/set"
#define T_MIST_COOLDOWN_SET      "monotub/mist/cooldown_seconds/set"
#define T_FAN_COOLDOWN_SET       "monotub/fan/cooldown_seconds/set"
#define T_SIM_ENABLED_SET        "monotub/simulation/enabled/set"
#define T_SIM_HUMIDITY_SET       "monotub/simulation/humidity/set"

// Intervals
constexpr uint32_t STATUS_INTERVAL_MS = 10000;
constexpr uint32_t ENV_INTERVAL_MS = 5000;
// Fan schedule must tick often enough for short ON windows (e.g. 30s).
constexpr uint32_t CONTROLLER_INTERVAL_MS = 1000;
constexpr uint32_t RELAY_BOOT_DELAY_MS = 5000;
constexpr uint32_t MQTT_RETRY_MS = 5000;
constexpr uint32_t WIFI_RETRY_MS = 10000;

// Publish immediately when telemetry moves enough to matter.
constexpr float ENV_HUMIDITY_PUBLISH_DELTA = 0.5F;
constexpr float ENV_TEMPERATURE_PUBLISH_DELTA = 0.2F;
