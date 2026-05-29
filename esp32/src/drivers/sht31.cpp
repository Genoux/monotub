#include "drivers/sht31.h"
#include "app/config.h"
#include "app/state.h"
#include <Adafruit_SHT31.h>
#include <Wire.h>
#include <math.h>

namespace {
Adafruit_SHT31 sensor;

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
  if (!sensor.begin(SHT31_I2C_ADDR)) {
    Serial.println("SHT31 init failed");
    g.environment.ok = false;
    return false;
  }
  Serial.println("SHT31 ready");
  g.environment.ok = true;
  return true;
}

bool sht31Read() {
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
  return sht31Read();
}
