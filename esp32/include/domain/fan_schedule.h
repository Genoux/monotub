#pragma once

// Start or resume routine using g.fanSettings.scheduledOnSeconds / scheduledOffSeconds.
void fanScheduleBegin();
void fanScheduleResyncTimer();
void fanScheduleTick();
