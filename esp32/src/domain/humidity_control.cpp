#include "domain/humidity_control.h"
#include "app/state.h"

namespace {
uint32_t elapsedSecondsSince(uint32_t startedAt) {
  if (startedAt == 0) return UINT32_MAX;
  return (millis() - startedAt) / 1000UL;
}

bool cooldownReady(uint32_t lastAt, uint32_t cooldownSeconds) {
  return lastAt == 0 || elapsedSecondsSince(lastAt) >= cooldownSeconds;
}

bool pulseActive(uint32_t until) {
  return until != 0 && (int32_t)(until - millis()) > 0;
}

uint32_t adaptiveHighPersistSeconds(float rh) {
  const uint32_t configured = g.humiditySettings.highPersistSeconds;
  const float severeRh = g.humiditySettings.highRhPercent + 2.0F;
  const float criticalRh = g.humiditySettings.highRhPercent + 4.0F;
  if (rh >= criticalRh) {
    return min(configured, HIGH_RH_CRITICAL_PERSIST_SECONDS);
  }
  if (rh >= severeRh) {
    return min(configured, HIGH_RH_SEVERE_PERSIST_SECONDS);
  }
  return configured;
}

bool pulseWindowAllows(uint32_t now) {
  if (g.runtime.correctiveFanPulseWindowStartedAt == 0 ||
      elapsedSecondsSince(g.runtime.correctiveFanPulseWindowStartedAt) >=
          CORRECTIVE_FAN_PULSE_WINDOW_SECONDS) {
    g.runtime.correctiveFanPulseWindowStartedAt = now;
    g.runtime.correctiveFanPulseCount = 0;
  }
  return g.runtime.correctiveFanPulseCount <
         MAX_CORRECTIVE_FAN_PULSES_PER_HOUR;
}

}  // namespace

void humidityControlTick() {
  if (!g.humiditySettings.automationEnabled || !g.environment.ok ||
      isnan(g.environment.smoothedHumidityPercent)) {
    g.runtime.lowHumiditySince = 0;
    g.runtime.highHumiditySince = 0;
    return;
  }

  const uint32_t now = millis();
  const float rh = g.environment.smoothedHumidityPercent;
  const uint32_t persistSeconds = adaptiveHighPersistSeconds(rh);

  g.runtime.lowHumiditySince = 0;

  if (rh > g.humiditySettings.highRhPercent) {
    if (g.runtime.highHumiditySince == 0) g.runtime.highHumiditySince = now;
  } else {
    g.runtime.highHumiditySince = 0;
  }

  const bool fanActive = pulseActive(g.runtime.correctiveFanUntil);
  const bool highPersisted =
      g.runtime.highHumiditySince != 0 &&
      elapsedSecondsSince(g.runtime.highHumiditySince) >= persistSeconds;

  if (highPersisted && !fanActive &&
      cooldownReady(g.runtime.lastCorrectiveFanAt, g.humiditySettings.fanCooldownSeconds) &&
      pulseWindowAllows(now)) {
    g.runtime.correctiveFanUntil = now + g.humiditySettings.fanPulseSeconds * 1000UL;
    g.runtime.lastCorrectiveFanAt = now;
    g.runtime.correctiveFanPulseCount++;
    g.runtime.highHumiditySince = 0;
    Serial.printf(
        "Humidity %.1f%% > %.1f%% for %us: corrective fan pulse %u/%u\n",
        rh, g.humiditySettings.highRhPercent, persistSeconds,
        g.runtime.correctiveFanPulseCount,
        MAX_CORRECTIVE_FAN_PULSES_PER_HOUR);
  }
}

uint32_t humidityFanCooldownRemainingSeconds() {
  if (g.runtime.lastCorrectiveFanAt == 0) return 0;
  uint32_t elapsed = elapsedSecondsSince(g.runtime.lastCorrectiveFanAt);
  return elapsed >= g.humiditySettings.fanCooldownSeconds
             ? 0
             : g.humiditySettings.fanCooldownSeconds - elapsed;
}

uint32_t humidityMistCooldownRemainingSeconds() {
  return 0;
}

uint32_t humidityHighPersistRemainingSeconds() {
  if (g.runtime.highHumiditySince == 0) return 0;
  const float rh = g.environment.smoothedHumidityPercent;
  if (isnan(rh)) return 0;
  const uint32_t persistSeconds = adaptiveHighPersistSeconds(rh);
  uint32_t elapsed = elapsedSecondsSince(g.runtime.highHumiditySince);
  return elapsed >= persistSeconds
             ? 0
             : persistSeconds - elapsed;
}

uint8_t humidityCorrectiveFanPulseCount() {
  return g.runtime.correctiveFanPulseCount;
}

uint8_t humidityCorrectiveFanPulseLimit() {
  return MAX_CORRECTIVE_FAN_PULSES_PER_HOUR;
}
