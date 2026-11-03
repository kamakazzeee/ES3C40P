#include "ui/ui.h"

#include "hardware/battery.h"

TFT_eSPI display;

namespace {
String displayedBatteryStatus;
}  // namespace

void beginDisplay() {
  display.init();
  display.setRotation(1);
}

void drawHeader(const char *title) {
  display.fillScreen(TFT_BLACK);
  display.setTextDatum(TL_DATUM);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.drawString("ES3C40P", 18, 16, 4);
  drawBatteryStatus();
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString(title, 18, 52, 2);
  display.drawFastHLine(18, 78, 444, TFT_DARKGREY);
}

void drawBatteryStatus(bool onlyIfChanged) {
  String status = batteryStatusText();
  if (onlyIfChanged && status == displayedBatteryStatus) return;
  display.fillRect(322, 10, 140, 24, TFT_BLACK);
  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.drawRightString(status, 460, 18, 2);
  displayedBatteryStatus = status;
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
