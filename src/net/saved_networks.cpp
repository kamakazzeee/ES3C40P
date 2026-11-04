#include "net/saved_networks.h"

#include <Preferences.h>

namespace {
constexpr const char *storageNamespace = "wifi";

String ssids[maxSavedNetworks];
String passwords[maxSavedNetworks];
uint8_t count = 0;

void writeSavedNetworks() {
  Preferences storage;
  storage.begin(storageNamespace, false);
  storage.putUChar("count", count);
  for (uint8_t i = 0; i < count; ++i) {
    storage.putString(("ssid" + String(i)).c_str(), ssids[i]);
    storage.putString(("pass" + String(i)).c_str(), passwords[i]);
  }
  // Erase slots freed by forgetNetwork() so a forgotten password does not stay in flash
  for (uint8_t i = count; i < maxSavedNetworks; ++i) {
    String ssidKey = "ssid" + String(i);
    String passwordKey = "pass" + String(i);
    if (storage.isKey(ssidKey.c_str())) storage.remove(ssidKey.c_str());
    if (storage.isKey(passwordKey.c_str())) storage.remove(passwordKey.c_str());
  }
  storage.end();
}
}  // namespace

// Opened read-write so the first boot creates the namespace instead of logging NOT_FOUND
void loadSavedNetworks() {
  Preferences storage;
  storage.begin(storageNamespace, false);
  count = min<uint8_t>(storage.getUChar("count", 0), maxSavedNetworks);
  for (uint8_t i = 0; i < count; ++i) {
    ssids[i] = storage.getString(("ssid" + String(i)).c_str(), "");
    passwords[i] = storage.getString(("pass" + String(i)).c_str(), "");
  }
  storage.end();
  Serial.printf("Wi-Fi: %u saved networks\n", count);
}

uint8_t savedNetworkCount() {
  return count;
}

String savedNetworkSsid(uint8_t index) {
  return index < count ? ssids[index] : String();
}

bool findSavedNetwork(const String &ssid, String &password) {
  for (uint8_t i = 0; i < count; ++i) {
    if (ssids[i] != ssid) continue;
    password = passwords[i];
    return true;
  }
  return false;
}

// When full, the least recently joined network drops off the end
void saveNetwork(const String &ssid, const String &password) {
  uint8_t existing = 0;
  while (existing < count && ssids[existing] != ssid) ++existing;
  if (existing == count && count < maxSavedNetworks) ++count;
  for (uint8_t i = min<uint8_t>(existing, count - 1); i > 0; --i) {
    ssids[i] = ssids[i - 1];
    passwords[i] = passwords[i - 1];
  }
  ssids[0] = ssid;
  passwords[0] = password;
  writeSavedNetworks();
}

void forgetNetwork(const String &ssid) {
  uint8_t index = 0;
  while (index < count && ssids[index] != ssid) ++index;
  if (index == count) return;
  for (uint8_t i = index; i + 1 < count; ++i) {
    ssids[i] = ssids[i + 1];
    passwords[i] = passwords[i + 1];
  }
  --count;
  writeSavedNetworks();
}
