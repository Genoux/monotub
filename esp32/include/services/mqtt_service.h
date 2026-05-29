#pragma once

#include "domain/actuators.h"

void mqttInit();
bool mqttConnect();
void mqttMaintain();
void mqttPublishLog(const char* fmt, ...);
void mqttPublishFanState();
void mqttPublishMistState();
void mqttPublishActuatorChanges(const ActuatorChanges& changes);
void mqttPublishStatus();
void mqttTickStatus();
