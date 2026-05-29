#pragma once

struct ActuatorChanges {
  bool fanChanged = false;
  bool mistChanged = false;
};

ActuatorChanges actuatorsApply(bool fanOn, bool mistOn);
ActuatorChanges actuatorsToggleManualFan();
ActuatorChanges actuatorsToggleManualMist();
void actuatorsClearManualFanHold();
void actuatorsClearManualMistHold();
void actuatorsSetManualFan(bool on);
void actuatorsSetManualMist(bool on);
