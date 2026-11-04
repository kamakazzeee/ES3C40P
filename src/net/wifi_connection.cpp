#include "net/wifi_connection.h"

#include <WiFi.h>

#include "net/saved_networks.h"
#include "wifi_secrets.h"

namespace {
constexpr unsigned long autoJoinTimeoutMs = 15000;

enum class AutoConnect : uint8_t { idle, scanning, joining, failed };
AutoConnect autoConnect = AutoConnect::idle;

String targetSsid;  // the network being joined, or already joined
unsigned long attemptStartedAt = 0;

void startConnecting(const String &ssid, const String &password) {
  targetSsid = ssid;
  attemptStartedAt = millis();
  WiFi.begin(ssid.c_str(), password.isEmpty() ? nullptr : password.c_str());
}

bool inScanResults(const String &ssid, int16_t found) {
  for (int16_t i = 0; i < found; ++i) {
    if (WiFi.SSID(i) == ssid) return true;
  }
  return false;
}

bool pickKnownNetwork(int16_t found, String &ssid, String &password) {
  if (inScanResults(WIFI_SSID, found)) {
    ssid = WIFI_SSID;
    password = WIFI_PASSWORD;
    return true;
  }
  for (uint8_t i = 0; i < savedNetworkCount(); ++i) {
    if (!inScanResults(savedNetworkSsid(i), found)) continue;
    ssid = savedNetworkSsid(i);
    findSavedNetwork(ssid, password);
    return true;
  }
  return false;
}

void stopAttempt() {
  WiFi.disconnect();
  targetSsid = "";
  autoConnect = AutoConnect::failed;
}
}  // namespace

void beginWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  loadSavedNetworks();
  retryAutoConnect();
}

void retryAutoConnect() {
  WiFi.disconnect();
  targetSsid = "";
  autoConnect = WiFi.scanNetworks(true) == WIFI_SCAN_FAILED ? AutoConnect::failed : AutoConnect::scanning;
}

void updateWifi() {
  if (autoConnect == AutoConnect::scanning) {
    int16_t found = WiFi.scanComplete();
    if (found == WIFI_SCAN_RUNNING) return;
    String ssid;
    String password;
    if (found > 0 && pickKnownNetwork(found, ssid, password)) {
      Serial.printf("Wi-Fi: joining %s\n", ssid.c_str());
      startConnecting(ssid, password);
      autoConnect = AutoConnect::joining;
    } else {
      Serial.println("Wi-Fi: no known network in range");
      autoConnect = AutoConnect::failed;
    }
    WiFi.scanDelete();
  } else if (autoConnect == AutoConnect::joining) {
    if (wifiConnected()) {
      autoConnect = AutoConnect::idle;
    } else if (millis() - attemptStartedAt > autoJoinTimeoutMs) {
      Serial.printf("Wi-Fi: could not join %s\n", targetSsid.c_str());
      stopAttempt();
    }
  }
}

bool wifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

// idle with a target means the driver is reconnecting after a drop
bool wifiConnecting() {
  if (wifiConnected()) return false;
  return autoConnect == AutoConnect::scanning || autoConnect == AutoConnect::joining ||
         (autoConnect == AutoConnect::idle && !targetSsid.isEmpty());
}

String wifiStatusText() {
  if (wifiConnected()) {
    return "Wi-Fi: " + targetSsid + "  " + WiFi.localIP().toString();
  }
  if (autoConnect == AutoConnect::scanning) return "Wi-Fi: Looking for known networks";
  if (wifiConnecting()) return "Wi-Fi: Connecting to " + targetSsid;
  return "Wi-Fi: Not connected - tap to choose";
}

bool waitForWifi(unsigned long timeoutMs) {
  if (wifiConnected()) return true;
  if (targetSsid.isEmpty()) return false;
  Serial.println("Wi-Fi dropped; reconnecting");
  WiFi.reconnect();
  unsigned long startedAt = millis();
  while (!wifiConnected() && millis() - startedAt < timeoutMs) delay(250);
  return wifiConnected();
}

bool autoConnectFailed() {
  return autoConnect == AutoConnect::failed;
}

void prepareWifiScan() {
  while (WiFi.scanComplete() == WIFI_SCAN_RUNNING) delay(50);
  if (autoConnect == AutoConnect::scanning) WiFi.scanDelete();
  if (!wifiConnected()) stopAttempt();
}

bool knownNetworkPassword(const String &ssid, String &password) {
  if (ssid == WIFI_SSID) {
    password = WIFI_PASSWORD;
    return true;
  }
  return findSavedNetwork(ssid, password);
}

// begin() does not reset WiFi.status(), so a failure left over from an earlier attempt
// would look like this one failing; only the timeout is trusted
bool joinNetwork(const String &ssid, const String &password, unsigned long timeoutMs) {
  WiFi.disconnect();
  autoConnect = AutoConnect::idle;
  startConnecting(ssid, password);
  while (!wifiConnected() && millis() - attemptStartedAt < timeoutMs) delay(100);
  bool joined = wifiConnected();
  Serial.printf("Wi-Fi: %s %s\n", joined ? "joined" : "could not join", ssid.c_str());
  if (!joined) {
    stopAttempt();
    return false;
  }
  if (ssid != WIFI_SSID) saveNetwork(ssid, password);
  return true;
}
