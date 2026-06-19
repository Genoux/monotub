#pragma once

#include "domain/actuators.h"

void controllerInit();
ActuatorChanges controllerTick();
ActuatorChanges controllerApplyOutputs();
ActuatorChanges controllerForceApplyOutputs();
ActuatorChanges controllerSetAutomationEnabled(bool enabled);
ActuatorChanges controllerSetFanEnabled(bool enabled);
ActuatorChanges controllerToggleFan();
ActuatorChanges controllerSetMistEnabled(bool enabled);
ActuatorChanges controllerToggleMist();
