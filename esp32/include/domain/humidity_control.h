#pragma once

#include <cstdint>

void humidityControlTick();
uint32_t humidityFanCooldownRemainingSeconds();
uint32_t humidityMistCooldownRemainingSeconds();
uint32_t humidityHighPersistRemainingSeconds();
