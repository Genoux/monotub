#include "drivers/wifi_connect.h"
#include "app/config.h"
#include "app/state.h"
#include "secrets.h"
#include <WiFi.h>
#include <WiFiManager.h>

static_assert(sizeof(OTA_PASSWORD) > 8,
              "OTA_PASSWORD must be at least 8 characters: WPA2 rejects shorter ones and the hotspot would silently not start");

namespace {
WiFiManager setupPortal;
std::vector<const char*> setupMenu = {"wifi", "exit"};
}  // namespace

void wifiInit() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  // A fresh board has nothing saved yet; secrets.h seeds the first network, which the
  // Wi-Fi driver then keeps in flash like any network chosen through the hotspot.
  if (!setupPortal.getWiFiIsSaved()) WiFi.begin(WIFI_SSID, WIFI_PASS);

  setupPortal.setDebugOutput(false);
  // Non-blocking: the control loop keeps regulating humidity while the hotspot is open.
  setupPortal.setConfigPortalBlocking(false);
  setupPortal.setConnectTimeout(WIFI_CONNECT_TIMEOUT_S);
  setupPortal.setConfigPortalTimeout(WIFI_SETUP_PORTAL_TIMEOUT_S);
  // Hides the portal's firmware-upload link: it bypasses the relays-off guard of ArduinoOTA (services/ota.cpp).
  setupPortal.setMenu(setupMenu);

  Serial.print("WiFi ");
  // WPA2 hotspot keyed with the OTA password: an open one would let anyone nearby point the
  // board at their own broker and drive the relays.
  if (setupPortal.autoConnect(WIFI_SETUP_AP_NAME, OTA_PASSWORD)) {
    Serial.printf("%s %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  } else {
    Serial.printf("not reachable: setup hotspot \"%s\" open for %us\n", WIFI_SETUP_AP_NAME,
                  WIFI_SETUP_PORTAL_TIMEOUT_S);
  }
}

void wifiMaintain() {
  if (setupPortal.process()) {
    Serial.printf("WiFi joined %s %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  }
  if (setupPortal.getConfigPortalActive() || WiFi.isConnected()) return;
  if (millis() - g.runtime.lastWifiRetry > WIFI_RETRY_MS) {
    g.runtime.lastWifiRetry = millis();
    WiFi.reconnect();
  }
}
