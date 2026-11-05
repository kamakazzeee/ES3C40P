#include "ui/ui.h"

#include "hardware/battery.h"
#include "hardware/board.h"
#include "hardware/sd_storage.h"
#include "net/wifi_connection.h"

TFT_eSPI display;

namespace {
constexpr unsigned long statusCheckIntervalMs = 1000;

struct Status {
  uint8_t wifiBars;
  bool wifiConnecting;
  bool sdReady;
  bool batteryFitted;
  uint8_t batteryPercent;
};

bool headerShown = false;  // false while a screen without the header (the keyboard) is up
Status shownStatus = {};
unsigned long lastStatusCheckAt = 0;

Status readStatus() {
  bool fitted = batteryConnected();
  return {wifiSignalBars(), wifiConnecting(), sdCardReady(), fitted, fitted ? batteryPercent(smoothedBatteryVoltage()) : uint8_t(0)};
}

bool sameStatus(const Status &a, const Status &b) {
  return a.wifiBars == b.wifiBars && a.wifiConnecting == b.wifiConnecting && a.sdReady == b.sdReady &&
         a.batteryFitted == b.batteryFitted && a.batteryPercent == b.batteryPercent;
}

// White bars up to the signal strength; orange while connecting; grey with a red cross when offline
void drawWifiMeter(int16_t left, int16_t bottom, const Status &status) {
  for (uint8_t bar = 0; bar < 4; ++bar) {
    int16_t height = 4 + bar * 4;
    uint16_t colour = status.wifiConnecting ? TFT_ORANGE : bar < status.wifiBars ? TFT_WHITE : TFT_DARKGREY;
    display.fillRect(left + bar * 6, bottom - height, 4, height, colour);
  }
  if (status.wifiBars == 0 && !status.wifiConnecting) {
    for (int16_t thick = 0; thick < 2; ++thick) {
      display.drawLine(left + thick, bottom - 16, left + 6 + thick, bottom - 10, TFT_RED);
      display.drawLine(left + 6 + thick, bottom - 16, left + thick, bottom - 10, TFT_RED);
    }
  }
}

// A card with a clipped corner and contacts; green when mounted, grey and struck through when not
void drawSdIcon(int16_t left, int16_t top, bool ready) {
  display.fillRect(left, top, 14, 18, ready ? TFT_GREEN : TFT_DARKGREY);
  display.fillTriangle(left + 9, top, left + 14, top, left + 14, top + 5, TFT_BLACK);
  for (int16_t contact = 0; contact < 3; ++contact) {
    display.fillRect(left + 2 + contact * 3, top + 2, 2, 4, TFT_BLACK);
  }
  if (!ready) {
    for (int16_t thick = 0; thick < 2; ++thick) {
      display.drawLine(left - 2 + thick, top + 19, left + 15 + thick, top - 1, TFT_RED);
    }
  }
}

void drawBatteryIcon(int16_t left, int16_t top, uint8_t percent) {
  uint16_t colour = percent > 50 ? TFT_GREEN : percent > 20 ? TFT_YELLOW : TFT_RED;
  display.drawRect(left, top, 24, 13, TFT_WHITE);
  display.fillRect(left + 24, top + 4, 2, 5, TFT_WHITE);
  display.fillRect(left + 2, top + 2, (20 * percent + 50) / 100, 9, colour);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawRightString(String(percent) + "%", 462, 15, 2);
}

// Shown instead of the battery when none is plugged in: a mains plug and "USB"
void drawPlugIcon(int16_t left, int16_t top) {
  display.fillRect(left + 1, top + 2, 6, 2, TFT_LIGHTGREY);
  display.fillRect(left + 1, top + 9, 6, 2, TFT_LIGHTGREY);
  display.fillRoundRect(left + 7, top, 10, 13, 3, TFT_LIGHTGREY);
  display.fillRect(left + 17, top + 5, 8, 3, TFT_LIGHTGREY);
  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  display.drawRightString("USB", 462, 15, 2);
}

void drawStatusIcons(const Status &status) {
  display.fillRect(340, 8, 124, 28, TFT_BLACK);
  drawWifiMeter(352, 31, status);
  drawSdIcon(380, 13, status.sdReady);
  if (status.batteryFitted) {
    drawBatteryIcon(400, 16, status.batteryPercent);
  } else {
    drawPlugIcon(400, 16);
  }
  shownStatus = status;
}
}  // namespace

void beginDisplay() {
  display.init();
  display.setRotation(1);
  display.initDMA();  // used by pushImageDMA() in picture.cpp; ordinary drawing is unaffected
}

void drawHeader(const char *title) {
  display.fillScreen(TFT_BLACK);
  display.setTextDatum(TL_DATUM);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.drawString("ES3C40P", 18, 16, 4);
  drawStatusIcons(readStatus());
  lastStatusCheckAt = millis();
  headerShown = true;
  if (title) {
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawString(title, 18, 52, 2);
  }
  display.drawFastHLine(18, title ? 78 : 46, 444, TFT_DARKGREY);
}

void markHeaderHidden() {
  headerShown = false;
}

void updateStatusIcons() {
  // Not while the backlight is off: nobody can see it, and the battery reads high with that load gone
  if (!headerShown || !backlightOn() || millis() - lastStatusCheckAt < statusCheckIntervalMs) return;
  lastStatusCheckAt = millis();
  Status status = readStatus();
  if (!sameStatus(status, shownStatus)) drawStatusIcons(status);
}

void drawButton(const Button &button, const char *label, uint16_t fill, uint16_t border,
                uint16_t textColor, bool centred) {
  display.fillRect(button.left, button.top, button.width, button.height, fill);
  display.drawRect(button.left, button.top, button.width, button.height, border);
  display.setTextColor(textColor, fill);
  int16_t textY = button.top + (button.height - display.fontHeight(2)) / 2;
  if (centred) {
    display.drawCentreString(label, button.left + button.width / 2, textY, 2);
  } else {
    display.drawString(label, button.left + 22, textY, 2);
  }
}

void drawBackButton() {
  drawButton(backButton, "BACK TO MENU", TFT_DARKGREY, TFT_LIGHTGREY, TFT_WHITE);
}

void drawSmoothCentre(const uint8_t *font, const String &text, int16_t y, uint16_t color) {
  display.loadFont(font);
  display.setTextColor(color, TFT_BLACK);
  display.setTextDatum(TC_DATUM);
  display.drawString(text, 240, y);
  display.setTextDatum(TL_DATUM);
  display.unloadFont();
}
