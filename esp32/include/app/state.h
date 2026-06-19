#pragma once

#include "app/config.h"
#include <Arduino.h>

enum class FanReason {
  Off,
  Manual,
  Vent,
};

enum class MistReason {
  Manual,
  HumidityAutomation,
};

enum class HumidityBand {
  Unknown,
  TooDry,
  Recovering,
  Healthy,
  Drying,
  TooWet,
};

struct EnvironmentState {
  float humidityPercent = NAN;
  float temperatureC = NAN;
  float smoothedHumidityPercent = NAN;
  float simulatedHumidityPercent = 88.0F;
  uint8_t sampleCount = 0;
  bool ok = false;
  bool simulationEnabled = false;
  uint32_t lastReadAt = 0;
};

struct FanSettings {
  uint32_t ventOnSeconds = DEFAULT_VENT_ON_SECONDS;
  uint32_t ventOffSeconds = DEFAULT_VENT_OFF_SECONDS;
};

struct HumiditySettings {
  bool automationEnabled = DEFAULT_AUTOMATION_ENABLED;
  float rhDryBelow = DEFAULT_RH_DRY_BELOW;
  float rhBandLow = DEFAULT_RH_BAND_LOW;
  float rhBandHigh = DEFAULT_RH_BAND_HIGH;
  float rhWetAbove = DEFAULT_RH_WET_ABOVE;
  uint32_t bandHoldSeconds = DEFAULT_BAND_HOLD_SECONDS;
};

struct ActuatorState {
  bool fanOn = false;
  bool mistOn = false;
  bool manualFanOn = false;
  bool manualMistOn = true;
  bool automationMistOn = true;
  bool dutyFanOn = false;
  FanReason fanReason = FanReason::Off;
  MistReason mistReason = MistReason::Manual;
};

struct RuntimeState {
  // Fan duty-cycle engine.
  uint32_t nextDutyToggleAt = 0;
  uint32_t dutyOnSeconds = 0;   // active cycle signature (0 = stopped)
  uint32_t dutyOffSeconds = 0;
  // Humidity band debounce.
  HumidityBand currentBand = HumidityBand::Unknown;   // candidate from latest RH
  HumidityBand engagedBand = HumidityBand::Unknown;   // acted-upon band (after hold)
  uint32_t bandEnteredAt = 0;
  // Infrastructure timers.
  uint32_t lastStatusAt = 0;
  uint32_t lastControllerAt = 0;
  uint32_t lastMqttRetry = 0;
  uint32_t lastWifiRetry = 0;
  uint32_t relayCommandSuppressUntil = 0;
  uint32_t relayBootSuppressUntil = 0;
  uint32_t lastRelayCommandAt = 0;
};

struct MonotubState {
  EnvironmentState environment;
  FanSettings fanSettings;
  HumiditySettings humiditySettings;
  ActuatorState actuators;
  RuntimeState runtime;
};

extern MonotubState g;

const char* fanReasonName(FanReason reason);
const char* mistReasonName(MistReason reason);
