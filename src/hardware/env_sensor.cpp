#include "hardware/env_sensor.h"

#include <Adafruit_BME280.h>
#include <Adafruit_BMP280.h>
#include <Wire.h>

EnvSensor envSensor;

namespace {
Adafruit_BME280 bme280;
Adafruit_BMP280 bmp280;
}  // namespace

void beginEnvSensor() {
  if (bme280.begin(0x76, &Wire)) {
    envSensor.available = true;
    envSensor.hasHumidity = true;
    envSensor.address = 0x76;
    envSensor.model = "BME280";
  } else if (bme280.begin(0x77, &Wire)) {
    envSensor.available = true;
    envSensor.hasHumidity = true;
    envSensor.address = 0x77;
    envSensor.model = "BME280";
  } else if (bmp280.begin(0x76)) {
    envSensor.available = true;
    envSensor.address = 0x76;
    envSensor.model = "BMP280";
  } else if (bmp280.begin(0x77)) {
    envSensor.available = true;
    envSensor.address = 0x77;
    envSensor.model = "BMP280";
  }

  if (envSensor.available) {
    Serial.printf("%s sensor detected at 0x%02X\n", envSensor.model.c_str(), envSensor.address);
  } else {
    Serial.println("No BME280 or BMP280 sensor detected");
  }
}

void updateEnvSensor() {
  if (envSensor.hasHumidity) {
    envSensor.temperature = bme280.readTemperature();
    envSensor.pressure = bme280.readPressure() / 100.0f;
    envSensor.humidity = bme280.readHumidity();
  } else if (envSensor.available) {
    envSensor.temperature = bmp280.readTemperature();
    envSensor.pressure = bmp280.readPressure() / 100.0f;
  }
  envSensor.readingValid = envSensor.available && !isnan(envSensor.temperature) &&
                           !isnan(envSensor.pressure);
}
