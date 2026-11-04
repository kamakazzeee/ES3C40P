#pragma once
#include <Arduino.h>

// Networks joined from the Wi-Fi screen, kept in flash (NVS) across reboots. Passwords are
// stored unencrypted, the same as the home network compiled in from wifi_secrets.h.
constexpr uint8_t maxSavedNetworks = 5;

void loadSavedNetworks();
uint8_t savedNetworkCount();
String savedNetworkSsid(uint8_t index);  // 0 is the most recently joined
bool findSavedNetwork(const String &ssid, String &password);
void saveNetwork(const String &ssid, const String &password);  // moves it to the front
