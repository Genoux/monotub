#pragma once

#include <cstddef>
#include <cstdint>

constexpr size_t MQTT_HOST_MAX = 63;

// Plain C++ (no Arduino) so esp32/test/broker_address_test.cpp runs on the host.
inline bool parseBrokerPort(const char* text, uint16_t& port) {
  if (text == nullptr || *text == '\0') return false;
  uint32_t value = 0;
  for (const char* digit = text; *digit != '\0'; ++digit) {
    if (*digit < '0' || *digit > '9') return false;
    value = value * 10 + static_cast<uint32_t>(*digit - '0');
    if (value > 65535) return false;
  }
  if (value == 0) return false;
  port = static_cast<uint16_t>(value);
  return true;
}
