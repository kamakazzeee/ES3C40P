#pragma once
#include <Arduino.h>

void beginBattery();
float batteryVoltage();
uint8_t batteryPercent(float voltage);
// Averaged over about a minute, for display: the reading rises when the load drops (turning the
// backlight off alone lifts it) and sags back over a few seconds. Call about once a second.
float smoothedBatteryVoltage();
// False when no battery is plugged in (the pin then reads the charger's ~4.1 V, not a battery).
// Re-checked at most every 10 s; a check takes ~64 ms.
bool batteryConnected();
