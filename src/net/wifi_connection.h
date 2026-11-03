#pragma once
#include <Arduino.h>

void beginWifi();  // start connecting to the network in wifi_secrets.h
bool wifiConnected();
bool wifiConnecting();
String wifiStatusText();  // e.g. "Wi-Fi: MyNetwork  192.168.1.20"
bool waitForWifi(unsigned long timeoutMs);  // reconnect if needed; true once connected
