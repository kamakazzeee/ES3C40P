#pragma once
#include <Arduino.h>

// Every pin and I2C address on the ES3C40P, in one place. See the pin map in CLAUDE.md.

// Shared I2C bus: touch 0x38, ES8311 codec 0x18, BMP280 sensor 0x76
constexpr uint8_t i2cSdaPin = 16;
constexpr uint8_t i2cSclPin = 15;

constexpr uint8_t touchAddress = 0x38;
constexpr uint8_t touchResetPin = 18;
constexpr uint8_t touchInterruptPin = 17;

constexpr uint8_t backlightPin = 45;
constexpr uint8_t rgbLedPin = 42;
constexpr uint8_t bootButtonPin = 0;
constexpr uint8_t batteryAdcPin = 9;

constexpr int sdClockPin = 38;
constexpr int sdCommandPin = 40;
constexpr int sdData0Pin = 39;
constexpr int sdData1Pin = 41;
constexpr int sdData2Pin = 48;
constexpr int sdData3Pin = 47;

constexpr uint8_t codecAddress = 0x18;
constexpr uint8_t amplifierEnablePin = 1;  // FM8002E, low = on
constexpr int i2sMasterClockPin = 4;
constexpr int i2sBitClockPin = 5;
constexpr int i2sWordSelectPin = 7;
constexpr int i2sDataOutPin = 8;  // ESP32 -> ES8311 DSDIN
constexpr int i2sDataInPin = 6;   // ES8311 ASDOUT -> ESP32
