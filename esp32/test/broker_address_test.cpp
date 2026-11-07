#include "app/broker_address.h"

#include <cassert>
#include <cstdio>

int main() {
  uint16_t port = 1;
  assert(parseBrokerPort("1883", port) && port == 1883);
  assert(parseBrokerPort("65535", port) && port == 65535);
  assert(parseBrokerPort("1", port) && port == 1);

  port = 42;
  assert(!parseBrokerPort("70000", port) && port == 42);
  assert(!parseBrokerPort("65536", port) && port == 42);
  assert(!parseBrokerPort("0", port) && port == 42);
  assert(!parseBrokerPort("1883abc", port) && port == 42);
  assert(!parseBrokerPort("-1", port) && port == 42);
  assert(!parseBrokerPort("", port) && port == 42);
  assert(!parseBrokerPort(nullptr, port) && port == 42);
  assert(!parseBrokerPort("99999999999999999999", port) && port == 42);

  std::puts("broker_address_test: ok");
  return 0;
}
