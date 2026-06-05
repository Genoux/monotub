#pragma once

#include <cstdint>

void humidityControlTick();
uint32_t humidityFanCooldownRemainingSeconds();
uint32_t humidityMistCooldownRemainingSeconds();
uint32_t humidityHighPersistRemainingSeconds();
uint8_t humidityCorrectiveFanPulseCount();
uint8_t humidityCorrectiveFanPulseLimit();
