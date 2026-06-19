#pragma once

#include "app/state.h"
#include <cstdint>

enum class MistIntent { On, Off, Hold };
enum class FanMode { Off, Vent };

// Pure classification of the linear band model. No side effects.
HumidityBand classifyBand(float rh);
MistIntent mistIntentForBand(HumidityBand band);
FanMode fanModeForBand(HumidityBand band);
const char* humidityBandName(HumidityBand band);

// Seconds left before a pending band transition engages (0 when settled).
uint32_t bandHoldRemainingSeconds();
