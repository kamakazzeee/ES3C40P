#pragma once
#include <Arduino.h>

// BMP280 (this unit) or BME280 temperature and pressure sensor on the I2C header
struct EnvSensor {
  bool available = false;
  bool hasHumidity = false;  // only the BME280 measures humidity
  bool readingValid = false;
  uint8_t address = 0;
  String model;
  float temperature = 0;  // C
  float pressure = 0;     // hPa
  float humidity = 0;     // %

  float temperatureFahrenheit() const { return temperature * 9.0f / 5.0f + 32.0f; }
};

extern EnvSensor envSensor;

void beginEnvSensor();   // Wire must already be started
void updateEnvSensor();  // take a fresh reading into envSensor
