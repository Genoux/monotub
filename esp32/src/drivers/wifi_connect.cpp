#include "drivers/wifi_connect.h"
#include "secrets.h"
#include <WiFi.h>

void wifiInit() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.printf("WiFi %s ", WIFI_SSID);
  uint32_t start = millis();
  while (!WiFi.isConnected() && millis() - start < 15000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println(WiFi.isConnected()
                     ? WiFi.localIP().toString()
                     : "FAILED (will retry)");
}
