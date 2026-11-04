#include "screens/menu_screen.h"

#include "app.h"
#include "hardware/env_sensor.h"
#include "hardware/sd_storage.h"
#include "net/wifi_connection.h"
#include "screens/image_screen.h"
#include "screens/mic_screen.h"
#include "screens/scanner_screen.h"
#include "screens/sensor_screen.h"
#include "screens/weather_screen.h"
#include "screens/wifi_screen.h"
#include "ui/ui.h"

MenuScreen menuScreen;

namespace {
constexpr unsigned long statusIntervalMs = 1000;
constexpr Button wifiStatusTapArea = {0, 272, 320, 48};  // left of the SD card status

struct MenuItem {
  Button button;
  const char *label;
  uint16_t fill;
  uint16_t border;
  Screen *screen;
};

// To add a menu option: write a new screen and add one line here
const MenuItem menuItems[] = {
    {{32, 84, 416, 34}, "CURRENT WEATHER", TFT_DARKCYAN, TFT_CYAN, &weatherScreen},
    {{32, 122, 416, 34}, "SD IMAGE VIEWER", TFT_DARKGREEN, TFT_GREEN, &imageScreen},
    {{32, 160, 416, 34}, "WI-FI SIGNAL SCANNER", TFT_DARKGREY, TFT_LIGHTGREY, &scannerScreen},
    {{32, 198, 416, 34}, "ENVIRONMENT SENSOR", TFT_DARKCYAN, TFT_CYAN, &sensorScreen},
    {{32, 236, 416, 34}, "MICROPHONE TEST", TFT_MAROON, TFT_RED, &micScreen},
};
}  // namespace

void MenuScreen::draw() {
  drawHeader("HOME");
  drawTemperatureStatus();
  for (const MenuItem &item : menuItems) {
    drawButton(item.button, item.label, item.fill, item.border, TFT_WHITE, false);
  }
  drawStatusBar();
}

void MenuScreen::onTap(uint16_t x, uint16_t y) {
  for (const MenuItem &item : menuItems) {
    if (item.button.contains(x, y)) {
      showScreen(*item.screen);
      return;
    }
  }
  if (wifiStatusTapArea.contains(x, y)) showScreen(wifiScreen);
}

void MenuScreen::update() {
  if (millis() - lastStatusDrawAt < statusIntervalMs) return;
  lastStatusDrawAt = millis();
  updateEnvSensor();
  drawTemperatureStatus();
  drawBatteryStatus(true);
  if (wifiStatusText() != displayedWifiStatus) drawStatusBar();
  if (!offeredWifiScreen && autoConnectFailed()) {
    offeredWifiScreen = true;
    showScreen(wifiScreen);
  }
}

void MenuScreen::drawTemperatureStatus() {
  display.fillRect(142, 10, 160, 24, TFT_BLACK);
  display.setTextColor(TFT_ORANGE, TFT_BLACK);
  if (envSensor.readingValid) {
    display.drawCentreString("Temp: " + String(envSensor.temperatureFahrenheit(), 1) + " F", 222, 18, 2);
  } else {
    display.drawCentreString("Temp: -- F", 222, 18, 2);
  }
}

// Wi-Fi status on the left, SD card status on the right; the Wi-Fi text is cut short to fit
void MenuScreen::drawStatusBar() {
  String fullWifiStatus = wifiStatusText();
  String wifiStatus = fullWifiStatus;
  String sdStatus = sdCardReady() ? "SD card ready" : "No SD card detected";
  int16_t sdLeft = 460 - display.textWidth(sdStatus, 2);
  int16_t maximumWifiWidth = sdLeft - 18 - 12;
  while (wifiStatus.length() > 3 && display.textWidth(wifiStatus + "...", 2) > maximumWifiWidth) {
    wifiStatus.remove(wifiStatus.length() - 1);
  }
  if (wifiStatus != fullWifiStatus) wifiStatus += "...";

  display.fillRect(18, 274, 444, 30, TFT_BLACK);
  if (wifiConnected()) {
    display.setTextColor(TFT_GREEN, TFT_BLACK);
  } else if (wifiConnecting()) {
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
  } else {
    display.setTextColor(TFT_RED, TFT_BLACK);
  }
  display.drawString(wifiStatus, 18, 282, 2);
  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.drawRightString(sdStatus, 460, 282, 2);
  displayedWifiStatus = fullWifiStatus;
}
