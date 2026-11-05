#include "net/wifi_connection.h"

#include <WiFi.h>

#include "net/saved_networks.h"
#include "wifi_secrets.h"

namespace {
constexpr unsigned long autoJoinTimeoutMs = 15000;
constexpr uint8_t maxScanAttempts = 3;
constexpr unsigned long scanRetryDelayMs = 1500;
constexpr unsigned long scanTimeoutMs = 15000;

enum class AutoConnect : uint8_t { idle, waitingToScan, scanning, joining, failed };
AutoConnect autoConnect = AutoConnect::idle;
uint8_t scanAttempts = 0;
unsigned long scanRetryFrom = 0;
unsigned long scanStartedAt = 0;

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

// The library reports an async scan as FAILED once 20 x 300 ms have passed, but a full scan
// here takes ~6.6 s and still finishes, so a FAILED that early is not believed
bool backgroundScanRunning() {
  int16_t found = WiFi.scanComplete();
  if (found == WIFI_SCAN_RUNNING) return true;
  return found == WIFI_SCAN_FAILED && autoConnect == AutoConnect::scanning &&
         millis() - scanStartedAt < scanTimeoutMs;
}

// An empty or failed result is retried a few times before giving up
void startScan() {
  ++scanAttempts;
  scanStartedAt = millis();
  if (WiFi.scanNetworks(true) == WIFI_SCAN_FAILED) {
    scanRetryFrom = millis();
    autoConnect = AutoConnect::waitingToScan;
  } else {
    autoConnect = AutoConnect::scanning;
  }
}

void retryOrGiveUp(int16_t found) {
  if (scanAttempts < maxScanAttempts) {
    Serial.printf("Wi-Fi: scan attempt %u found nothing, retrying\n", scanAttempts);
    scanRetryFrom = millis();
    autoConnect = AutoConnect::waitingToScan;
    return;
  }
  Serial.printf("Wi-Fi: no known network among %d in range\n", max<int16_t>(found, 0));
  autoConnect = AutoConnect::failed;
}
}  // namespace

// persistent(false) must come before mode(): mode() starts the radio and picks where it
// keeps settings then. Otherwise every begin() also writes the network to the radio's flash.
void beginWifi() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  loadSavedNetworks();
  retryAutoConnect();
}

void retryAutoConnect() {
  WiFi.disconnect();
  targetSsid = "";
  scanAttempts = 0;
  startScan();
}

void updateWifi() {
  if (autoConnect == AutoConnect::waitingToScan) {
    if (millis() - scanRetryFrom < scanRetryDelayMs) return;
    if (scanAttempts < maxScanAttempts) {
      startScan();
    } else {
      retryOrGiveUp(WIFI_SCAN_FAILED);
    }
  } else if (autoConnect == AutoConnect::scanning) {
    if (backgroundScanRunning()) return;
    int16_t found = WiFi.scanComplete();
    String ssid;
    String password;
    if (found > 0 && pickKnownNetwork(found, ssid, password)) {
      Serial.printf("Wi-Fi: joining %s\n", ssid.c_str());
      startConnecting(ssid, password);
      autoConnect = AutoConnect::joining;
    } else if (found <= 0) {
      retryOrGiveUp(found);
    } else {
      Serial.printf("Wi-Fi: no known network among %d in range\n", found);
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

uint8_t wifiSignalBars() {
  if (!wifiConnected()) return 0;
  int rssi = WiFi.RSSI();
  if (rssi >= -55) return 4;
  if (rssi >= -65) return 3;
  if (rssi >= -75) return 2;
  return 1;
}

// idle with a target means the driver is reconnecting after a drop
bool wifiConnecting() {
  if (wifiConnected()) return false;
  return autoConnect == AutoConnect::waitingToScan || autoConnect == AutoConnect::scanning ||
         autoConnect == AutoConnect::joining ||
         (autoConnect == AutoConnect::idle && !targetSsid.isEmpty());
}

String wifiStatusText() {
  if (wifiConnected()) {
    return "Wi-Fi: " + targetSsid + "  " + WiFi.localIP().toString();
  }
  if (autoConnect == AutoConnect::waitingToScan || autoConnect == AutoConnect::scanning) {
    return "Wi-Fi: Looking for known networks";
  }
  if (wifiConnecting()) return "Wi-Fi: Connecting to " + targetSsid;
  return "Wi-Fi: Not connected - tap to choose";
}

bool autoConnectFailed() {
  return autoConnect == AutoConnect::failed;
}

void prepareWifiScan() {
  while (backgroundScanRunning()) delay(50);
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

bool canForgetNetwork(const String &ssid) {
  String password;
  return ssid != WIFI_SSID && findSavedNetwork(ssid, password);
}

void forgetWifiNetwork(const String &ssid) {
  forgetNetwork(ssid);
  if (targetSsid == ssid) stopAttempt();
  Serial.printf("Wi-Fi: forgot %s\n", ssid.c_str());
}
