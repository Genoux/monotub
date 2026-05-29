#include "drivers/relay.h"
#include "app/config.h"

static void relayOff(uint8_t pin) {
  digitalWrite(pin, RELAY_OFF_LEVEL);
  pinMode(pin, OUTPUT);
}

void relayInit() {
  relayOff(FAN_RELAY_PIN);
  relayOff(MIST_RELAY_PIN);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
}

void relaySet(uint8_t pin, bool on) {
  digitalWrite(pin, on ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL);
  pinMode(pin, OUTPUT);
}
