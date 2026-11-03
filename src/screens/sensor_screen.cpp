#include "screens/sensor_screen.h"

#include "app.h"
#include "hardware/env_sensor.h"
#include "ui/ui.h"

SensorScreen sensorScreen;

void SensorScreen::enter() {
  updateEnvSensor();
  Serial.printf("Sensor %s at 0x%02X: temperature=%.2f C, pressure=%.2f hPa\n",
                envSensor.model.c_str(), envSensor.address, envSensor.temperature, envSensor.pressure);
}

void SensorScreen::draw() {
  drawHeader("ENVIRONMENT SENSOR");
  if (!envSensor.available) {
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.drawCentreString("SENSOR NOT DETECTED", 240, 145, 4);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawCentreString("Check the I2C connection", 240, 188, 2);
  } else if (!envSensor.readingValid) {
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.drawCentreString("SENSOR READ FAILED", 240, 145, 4);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawCentreString(envSensor.model + " connected at 0x" + String(envSensor.address, HEX), 240, 188, 2);
  } else {
    display.setTextColor(TFT_GREEN, TFT_BLACK);
    display.drawCentreString(envSensor.model + " 0x" + String(envSensor.address, HEX), 240, 92, 2);
    display.setTextColor(TFT_YELLOW, TFT_BLACK);
    display.drawString("Temperature: " + String(envSensor.temperature, 2) + " C / " +
                           String(envSensor.temperatureFahrenheit(), 2) + " F",
                       28, 130, 4);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawString("Pressure: " + String(envSensor.pressure, 2) + " hPa", 28, 180, 4);
    if (envSensor.hasHumidity) {
      display.drawString("Humidity: " + String(envSensor.humidity, 2) + "%", 28, 230, 4);
    } else {
      display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      display.drawString("Humidity: unavailable on BMP280", 28, 230, 2);
    }
  }
  drawBackButton();
}

void SensorScreen::onTap(uint16_t x, uint16_t y) {
  if (backButtonTapArea.contains(x, y)) showMenu();
}
