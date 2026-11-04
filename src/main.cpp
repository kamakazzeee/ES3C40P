// ES3C40P touchscreen hub: setup() starts each piece of hardware, loop() passes taps
// to whichever screen is showing. The screens live in src/screens/, the hardware
// drivers in src/hardware/.
#include <Arduino.h>
#include <Wire.h>

#include "app.h"
#include "hardware/audio.h"
#include "hardware/battery.h"
#include "hardware/board.h"
#include "hardware/env_sensor.h"
#include "hardware/sd_storage.h"
#include "hardware/touch.h"
#include "net/wifi_connection.h"
#include "pins.h"
#include "ui/ui.h"

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nES3C40P hardware demo");

  beginBoard();
  beginBattery();
  setRgb(0, 32, 32);
  beginTouch();

  Wire.begin(i2cSdaPin, i2cSclPin);
  Wire.setClock(100000);
  beginEnvSensor();
  updateEnvSensor();
  beginAudio();

  beginDisplay();
  beginSdCard();
  logSensorData();
  beginWifi();
  showMenu();
  playStartupChime();
}

void loop() {
  if (bootButtonPressed()) setBacklight(!backlightOn());

  uint16_t x;
  uint16_t y;
  if (readTap(x, y)) currentScreen().onTap(x, y);
  updateWifi();
  currentScreen().update();

  logSensorDataIfDue();
}
