#include "domain/humidity_control.h"
#include "app/state.h"
#include <Arduino.h>

// The entire humidity control law lives in one table:
//
//   Band        RH range            Mist    Fan
//   TooDry      < dry_below         On      Off
//   Recovering  dry_below..band_low Hold    Off
//   Healthy     band_low..band_high Hold    Off
//   Drying      band_high..wet_above Off    Vent
//   TooWet      >= wet_above        Off     Vent
//
// Fan runs only when humidity is out of range (Drying or TooWet).
// Airflow rises monotonically with humidity; mist falls monotonically with it.
// The Hold cells give the mist a deliberate hysteresis band (on below dry_below,
// off above band_high) so the relay never chatters around one number.

HumidityBand classifyBand(float rh) {
  const HumiditySettings& h = g.humiditySettings;
  if (rh < h.rhDryBelow) return HumidityBand::TooDry;
  if (rh < h.rhBandLow) return HumidityBand::Recovering;
  if (rh < h.rhBandHigh) return HumidityBand::Healthy;
  if (rh < h.rhWetAbove) return HumidityBand::Drying;
  return HumidityBand::TooWet;
}

MistIntent mistIntentForBand(HumidityBand band) {
  switch (band) {
    case HumidityBand::TooDry:
      return MistIntent::On;
    case HumidityBand::Drying:
    case HumidityBand::TooWet:
      return MistIntent::Off;
    default:
      return MistIntent::Hold;
  }
}

FanMode fanModeForBand(HumidityBand band) {
  switch (band) {
    case HumidityBand::Drying:
    case HumidityBand::TooWet:
      return FanMode::Vent;
    default:
      return FanMode::Off;
  }
}

const char* humidityBandName(HumidityBand band) {
  switch (band) {
    case HumidityBand::TooDry:
      return "too_dry";
    case HumidityBand::Recovering:
      return "recovering";
    case HumidityBand::Healthy:
      return "healthy";
    case HumidityBand::Drying:
      return "drying";
    case HumidityBand::TooWet:
      return "too_wet";
    case HumidityBand::Unknown:
    default:
      return "unknown";
  }
}

uint32_t bandHoldRemainingSeconds() {
  if (g.runtime.currentBand == g.runtime.engagedBand) return 0;
  if (g.runtime.bandEnteredAt == 0) return 0;
  const uint32_t deadline =
      g.runtime.bandEnteredAt + g.humiditySettings.bandHoldSeconds * 1000UL;
  const int32_t remaining = (int32_t)(deadline - millis());
  if (remaining <= 0) return 0;
  return ((uint32_t)remaining + 999UL) / 1000UL;  // ceiling so it reads 1 until it hits 0
}
