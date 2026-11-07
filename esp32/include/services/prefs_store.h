#pragma once

#include <cstdint>

void prefsLoad();
void prefsSave();
void prefsSaveRelayState();
void prefsSetMqttBroker(const char* host, uint16_t port);
