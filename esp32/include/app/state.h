#pragma once

#include "app/config.h"
#include <Arduino.h>

enum class FanReason {
  Off,
  Manual,
  ScheduledFae,
  HumidityCorrection,
};

enum class MistReason {
  Manual,
  HumidityAutomation,
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
  bool routineEnabled = DEFAULT_ROUTINE_ENABLED;
  uint32_t scheduledOnSeconds = DEFAULT_ON_SECONDS;
  uint32_t scheduledOffSeconds = DEFAULT_OFF_SECONDS;
};

struct HumiditySettings {
  bool automationEnabled = DEFAULT_AUTOMATION_ENABLED;
  float lowRhPercent = DEFAULT_RH_LOW_PERCENT;
  float highRhPercent = DEFAULT_RH_HIGH_PERCENT;
  uint32_t lowPersistSeconds = DEFAULT_RH_LOW_PERSIST_SECONDS;
  uint32_t highPersistSeconds = DEFAULT_RH_HIGH_PERSIST_SECONDS;
  uint32_t mistPulseSeconds = DEFAULT_MIST_PULSE_SECONDS;
  uint32_t fanPulseSeconds = DEFAULT_CORRECTIVE_FAN_SECONDS;
  uint32_t mistCooldownSeconds = DEFAULT_MIST_COOLDOWN_SECONDS;
  uint32_t fanCooldownSeconds = DEFAULT_FAN_COOLDOWN_SECONDS;
};

struct ActuatorState {
  bool fanOn = false;
  bool mistOn = false;
  bool manualFanOn = false;
  bool manualMistOn = true;
  bool automationMistOn = true;
  bool scheduledFanOn = false;
  FanReason fanReason = FanReason::Off;
  MistReason mistReason = MistReason::Manual;
};

struct RuntimeState {
  uint32_t nextScheduledFanToggleAt = 0;
  uint32_t lowHumiditySince = 0;
  uint32_t highHumiditySince = 0;
  uint32_t correctiveFanUntil = 0;
  uint32_t lastCorrectiveFanAt = 0;
  uint32_t lastStatusAt = 0;
  uint32_t lastControllerAt = 0;
  uint32_t lastMqttRetry = 0;
  uint32_t lastWifiRetry = 0;
  uint32_t relayCommandSuppressUntil = 0;
  uint32_t relayBootSuppressUntil = 0;
  uint32_t fanCommandLockUntil = 0;
  uint32_t lastRelayCommandAt = 0;
  bool fanRoutinePausedByHumidity = false;
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
