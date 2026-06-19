#pragma once

#include <cstdint>

// Generic fan duty-cycle engine. Drives g.actuators.dutyFanOn.
//
// Call every tick with the desired cycle:
//   active=false         -> fan off, cycle stopped.
//   active=true, startOn -> fan starts immediately in the ON phase.
//   active=true, !startOn -> fan waits the full OFF period before first ON.
//
// Changing (on,off) while already running keeps the cycle going uninterrupted.
// The startOn flag only matters when starting from a stopped state.
void fanDutyUpdate(bool active, uint32_t onSeconds, uint32_t offSeconds,
                   bool startOn = true);
