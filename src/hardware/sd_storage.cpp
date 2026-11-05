#include "hardware/sd_storage.h"

#include <SD_MMC.h>

#include "hardware/battery.h"
#include "hardware/env_sensor.h"
#include "pins.h"

namespace {
constexpr char sensorLogPath[] = "/sensor_log.csv";
constexpr unsigned long sensorLogIntervalMs = 300000;

bool sdAvailable = false;
unsigned long lastSensorLogAt = 0;

bool mountSdCard(bool oneBitMode) {
  if (SD_MMC.begin("/sdcard", oneBitMode, false, SDMMC_FREQ_HIGHSPEED, 5) && SD_MMC.cardType() != CARD_NONE) {
    return true;
  }
  SD_MMC.end();
  return false;
}
}  // namespace

// 4-bit at 40 MHz reads 5.1 MB/s here (1-bit at 20 MHz: 1.95 MB/s); 1-bit is the fallback for cards that need it.
// Cards without high-speed support are left at 20 MHz by the driver.
bool beginSdCard() {
  SD_MMC.setPins(sdClockPin, sdCommandPin, sdData0Pin, sdData1Pin, sdData2Pin, sdData3Pin);
  bool oneBitMode = false;
  sdAvailable = mountSdCard(oneBitMode);
  if (!sdAvailable) {
    oneBitMode = true;
    sdAvailable = mountSdCard(oneBitMode);
  }
  if (sdAvailable) {
    Serial.printf("SD card: ready (%s mode)\n", oneBitMode ? "1-bit" : "4-bit");
  } else {
    Serial.println("SD card: not detected");
  }
  return sdAvailable;
}

bool sdCardReady() {
  return sdAvailable;
}

void logSensorData() {
  lastSensorLogAt = millis();
  if (!sdAvailable) return;

  File logFile = SD_MMC.open(sensorLogPath, FILE_APPEND);
  if (!logFile) {
    Serial.printf("Could not open sensor log: %s\n", sensorLogPath);
    return;
  }

  if (logFile.size() == 0) {
    logFile.println("uptime_seconds,battery_voltage_v,battery_percent,temperature_c,temperature_f,pressure_hpa,humidity_percent,sensor_model,sensor_address");
  }

  bool valid = envSensor.readingValid;
  bool battery = batteryConnected();  // without one the pin reads the charger, so leave these blank
  float voltage = batteryVoltage();
  logFile.print(millis() / 1000);
  logFile.print(",");
  if (battery) logFile.print(voltage, 3);
  logFile.print(",");
  if (battery) logFile.print(batteryPercent(voltage));
  logFile.print(",");
  if (valid) logFile.print(envSensor.temperature, 2);
  logFile.print(",");
  if (valid) logFile.print(envSensor.temperatureFahrenheit(), 2);
  logFile.print(",");
  if (valid) logFile.print(envSensor.pressure, 2);
  logFile.print(",");
  if (envSensor.hasHumidity && valid) logFile.print(envSensor.humidity, 2);
  logFile.print(",");
  logFile.print(envSensor.available ? envSensor.model : "");
  logFile.print(",");
  if (envSensor.available) logFile.printf("0x%02X", envSensor.address);
  logFile.println();
  logFile.close();
  Serial.printf("Sensor data appended to %s\n", sensorLogPath);
}

void logSensorDataIfDue() {
  if (!sdAvailable || millis() - lastSensorLogAt < sensorLogIntervalMs) return;
  updateEnvSensor();
  logSensorData();
}
