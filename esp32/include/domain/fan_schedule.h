#pragma once

// Start or resume routine using g.fanSettings.scheduledOnSeconds / scheduledOffSeconds.
void fanScheduleBegin();
void fanScheduleResumeWithOffWait();
void fanSchedulePause();
void fanScheduleResyncTimer();
void fanScheduleTick();
