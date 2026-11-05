#pragma once
#include <Arduino.h>

void beginBattery();
float batteryVoltage();
uint8_t batteryPercent(float voltage);
