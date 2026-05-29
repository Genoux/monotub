#pragma once

#include "domain/actuators.h"

void controllerInit();
ActuatorChanges controllerTick();
ActuatorChanges controllerApplyOutputs();
