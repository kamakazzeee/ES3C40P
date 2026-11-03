#include "net/wifi_connection.h"

#include <WiFi.h>

#include "wifi_secrets.h"

void beginWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

bool wifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

bool wifiConnecting() {
  return WiFi.status() == WL_IDLE_STATUS || WiFi.status() == WL_DISCONNECTED;
}

String wifiStatusText() {
  if (wifiConnected()) {
    return "Wi-Fi: " + String(WIFI_SSID) + "  " + WiFi.localIP().toString();
  }
  if (wifiConnecting()) {
    return "Wi-Fi: Connecting to " + String(WIFI_SSID);
  }
  return "Wi-Fi: Disconnected";
}

bool waitForWifi(unsigned long timeoutMs) {
  if (wifiConnected()) return true;
  Serial.println("Wi-Fi dropped; reconnecting");
  WiFi.reconnect();
  unsigned long startedAt = millis();
  while (!wifiConnected() && millis() - startedAt < timeoutMs) delay(250);
  return wifiConnected();
}
