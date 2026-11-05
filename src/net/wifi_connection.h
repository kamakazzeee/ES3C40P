#pragma once
#include <Arduino.h>

// At boot a background scan looks for a known network: the home network in wifi_secrets.h
// first, then networks saved from the Wi-Fi screen, most recently joined first.
void beginWifi();
void updateWifi();  // call from loop(): finishes the background scan and connection attempt
bool wifiConnected();
bool wifiConnecting();
String wifiStatusText();  // e.g. "Wi-Fi: MyNetwork  192.168.1.20"
uint8_t wifiSignalBars();  // 0 when not connected, else 1-4 from the signal strength

bool autoConnectFailed();  // no known network in range, or it would not connect
void retryAutoConnect();
void prepareWifiScan();  // waits out a background scan and stops attempts that are not connected

bool knownNetworkPassword(const String &ssid, String &password);  // home or saved
// Blocks until connected or timed out; a network joined this way is saved
bool joinNetwork(const String &ssid, const String &password, unsigned long timeoutMs);
bool canForgetNetwork(const String &ssid);   // saved ones only; the home network is built in
void forgetWifiNetwork(const String &ssid);  // also disconnects if it is the current network
