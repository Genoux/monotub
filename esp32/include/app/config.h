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
#define SHT31_I2C_ADDR     0x44
#define SHT31_I2C_ADDR_ALT 0x45

// Relay NVS defaults — assume OFF (physical relay is de-energised on unknown state).
constexpr bool DEFAULT_RELAY_FAN_ON  = false;
constexpr bool DEFAULT_RELAY_MIST_ON = false;

// Vent duty cycle — short fan nudges when humidity is out of range.
constexpr uint32_t DEFAULT_VENT_ON_SECONDS = 5;
constexpr uint32_t DEFAULT_VENT_OFF_SECONDS = 90;

// Humidity automation — one linear band model.
// Bands by RH: <dry_below TooDry | dry_below..band_low Recovering |
//   band_low..band_high Healthy | band_high..wet_above Drying | >=wet_above TooWet.
constexpr bool DEFAULT_AUTOMATION_ENABLED = true;
constexpr float DEFAULT_RH_DRY_BELOW = 85.0F;
constexpr float DEFAULT_RH_BAND_LOW = 86.0F;
constexpr float DEFAULT_RH_BAND_HIGH = 88.0F;
constexpr float DEFAULT_RH_WET_ABOVE = 92.0F;
// How long a new band must hold before its action engages.
// Small hold keeps brief sensor spikes from triggering fan nudges.
constexpr uint32_t DEFAULT_BAND_HOLD_SECONDS = 10;

constexpr uint32_t SETTINGS_VERSION = 6;

// MQTT topics
#define T_LOG         "monotub/log"
#define T_STATE       "monotub/fan/state"
#define T_MIST_STATE  "monotub/mist/state"
#define T_STATUS      "monotub/status"
#define T_STATUS_REQ  "monotub/status/request"
#define T_RELAY_RESYNC "monotub/relay/resync"
#define T_CONNECTED   "monotub/connected"
#define T_FAN_SET     "monotub/fan/set"
#define T_MIST_SET    "monotub/mist/set"

#define T_AUTOMATION_ENABLED_SET "monotub/automation/enabled/set"
#define T_RH_DRY_BELOW_SET   "monotub/humidity/dry_below/set"
#define T_RH_BAND_LOW_SET    "monotub/humidity/band_low/set"
#define T_RH_BAND_HIGH_SET   "monotub/humidity/band_high/set"
#define T_RH_WET_ABOVE_SET   "monotub/humidity/wet_above/set"
#define T_BAND_HOLD_SET      "monotub/humidity/band_hold_seconds/set"
#define T_VENT_ON_SEC_SET    "monotub/fan/vent_on_seconds/set"
#define T_VENT_OFF_SEC_SET   "monotub/fan/vent_off_seconds/set"
#define T_SIM_ENABLED_SET        "monotub/simulation/enabled/set"
#define T_SIM_HUMIDITY_SET       "monotub/simulation/humidity/set"

// Intervals
constexpr uint32_t STATUS_INTERVAL_MS = 5000;
constexpr uint16_t MQTT_CLIENT_BUFFER_BYTES = 2048;
constexpr size_t STATUS_JSON_BYTES = 2048;
constexpr uint32_t ENV_INTERVAL_MS = 5000;
// Controller must tick often enough for short vent windows.
constexpr uint32_t CONTROLLER_INTERVAL_MS = 1000;
constexpr uint32_t RELAY_BOOT_DELAY_MS = 5000;
constexpr uint32_t MQTT_RETRY_MS = 5000;
constexpr uint32_t WIFI_RETRY_MS = 10000;

// Publish immediately when telemetry moves enough to matter.
constexpr float ENV_HUMIDITY_PUBLISH_DELTA = 0.5F;
constexpr float ENV_TEMPERATURE_PUBLISH_DELTA = 0.2F;
