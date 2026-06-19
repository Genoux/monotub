#include "drivers/sht31.h"
#include "app/config.h"
#include "app/state.h"
#include <Adafruit_SHT31.h>
#include <Wire.h>
#include <math.h>

namespace {
Adafruit_SHT31 sensor;

void logI2cScan() {
  Serial.printf("I2C scan SDA=%d SCL=%d:\n", SHT31_SDA_PIN, SHT31_SCL_PIN);
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  device 0x%02X\n", addr);
      found++;
    }
  }
  if (found == 0) {
    Serial.println("  no devices — check power, SDA/SCL, pull-ups");
  }
}

float smoothHumidity(float current, float next) {
  if (isnan(current)) return next;
  return current * 0.75F + next * 0.25F;
}

bool changedEnough(float previousHumidity, float previousTemperature,
                   float nextHumidity, float nextTemperature) {
  if (isnan(previousHumidity) || isnan(previousTemperature)) return true;
  return fabs(nextHumidity - previousHumidity) >= ENV_HUMIDITY_PUBLISH_DELTA ||
         fabs(nextTemperature - previousTemperature) >= ENV_TEMPERATURE_PUBLISH_DELTA;
}
}  // namespace

bool sht31Init() {
  Wire.begin(SHT31_SDA_PIN, SHT31_SCL_PIN);
  const uint8_t addresses[] = {SHT31_I2C_ADDR, SHT31_I2C_ADDR_ALT};
  for (uint8_t addr : addresses) {
    if (sensor.begin(addr)) {
      Serial.printf("SHT31 ready at 0x%02X\n", addr);
      g.environment.ok = true;
      return true;
    }
  }
  Serial.println("SHT31 init failed (tried 0x44 and 0x45)");
  logI2cScan();
  g.environment.ok = false;
  return false;
}

bool sht31Read() {
  if (g.environment.simulationEnabled) {
    g.environment.humidityPercent = g.environment.simulatedHumidityPercent;
    g.environment.smoothedHumidityPercent = g.environment.simulatedHumidityPercent;
    if (isnan(g.environment.temperatureC)) {
      g.environment.temperatureC = 23.0F;
    }
    g.environment.ok = true;
    g.environment.sampleCount = min<uint8_t>(g.environment.sampleCount + 1, 10);
    g.environment.lastReadAt = millis();
    Serial.printf("Env simulation: %.1f%% RH\n",
                  g.environment.simulatedHumidityPercent);
    return true;
  }

  if (!g.environment.ok) return false;

  float humidity = sensor.readHumidity();
  float temperature = sensor.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("SHT31 read failed");
    g.environment.ok = false;
    g.environment.lastReadAt = millis();
    return true;
  }

  bool meaningfulChange = changedEnough(
      g.environment.humidityPercent,
      g.environment.temperatureC,
      humidity,
      temperature);

  g.environment.humidityPercent = humidity;
  g.environment.temperatureC = temperature;
  g.environment.smoothedHumidityPercent =
      smoothHumidity(g.environment.smoothedHumidityPercent, humidity);
  g.environment.sampleCount = min<uint8_t>(g.environment.sampleCount + 1, 10);
  g.environment.lastReadAt = millis();
  Serial.printf("Env: %.1f%% RH  %.1f C\n", humidity, temperature);
  return meaningfulChange;
}

bool sht31Tick() {
  if (millis() - g.environment.lastReadAt <= ENV_INTERVAL_MS) return false;
  if (!g.environment.ok && !g.environment.simulationEnabled) {
    static uint32_t lastInitRetryAt = 0;
    if (millis() - lastInitRetryAt >= 30000UL) {
      lastInitRetryAt = millis();
      sht31Init();
    }
  }
  return sht31Read();
}
