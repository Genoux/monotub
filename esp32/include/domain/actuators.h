#pragma once

struct ActuatorChanges {
  bool fanChanged = false;
  bool mistChanged = false;
};

ActuatorChanges actuatorsApply(bool fanOn, bool mistOn);
ActuatorChanges actuatorsForceApply(bool fanOn, bool mistOn);
