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

  g.runtime.lowHumiditySince = 0;

  if (rh > g.humiditySettings.highRhPercent) {
    if (g.runtime.highHumiditySince == 0) g.runtime.highHumiditySince = now;
  } else {
    g.runtime.highHumiditySince = 0;
  }

  const bool fanActive = pulseActive(g.runtime.correctiveFanUntil);
  const bool highPersisted =
      g.runtime.highHumiditySince != 0 &&
      elapsedSecondsSince(g.runtime.highHumiditySince) >=
          g.humiditySettings.highPersistSeconds;

  if (highPersisted && !fanActive &&
      cooldownReady(g.runtime.lastCorrectiveFanAt, g.humiditySettings.fanCooldownSeconds)) {
    g.runtime.correctiveFanUntil = now + g.humiditySettings.fanPulseSeconds * 1000UL;
    g.runtime.lastCorrectiveFanAt = now;
    g.runtime.highHumiditySince = 0;
    Serial.printf("Humidity %.1f%% > %.1f%%: starting corrective fan pulse\n",
                  rh, g.humiditySettings.highRhPercent);
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
  uint32_t elapsed = elapsedSecondsSince(g.runtime.highHumiditySince);
  return elapsed >= g.humiditySettings.highPersistSeconds
             ? 0
             : g.humiditySettings.highPersistSeconds - elapsed;
}
