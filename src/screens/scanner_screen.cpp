#include "screens/scanner_screen.h"

#include <WiFi.h>

#include "app.h"
#include "ui/ui.h"

ScannerScreen scannerScreen;

namespace {
constexpr uint8_t networksPerPage = 4;
constexpr Button previousButton = {18, 218, 120, 32};
constexpr Button rescanButton = {150, 218, 180, 32};
constexpr Button nextButton = {342, 218, 120, 32};

void drawPageButton(const Button &button, const char *label, bool enabled) {
  drawButton(button, label, enabled ? TFT_DARKCYAN : TFT_DARKGREY,
             enabled ? TFT_CYAN : TFT_LIGHTGREY, enabled ? TFT_WHITE : TFT_LIGHTGREY);
}
}  // namespace

void ScannerScreen::enter() {
  drawHeader("WI-FI SIGNAL SCANNER");
  scanNetworks();
}

void ScannerScreen::scanNetworks() {
  display.fillRect(0, 82, 480, 182, TFT_BLACK);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawCentreString("Scanning networks...", 240, 145, 2);
  networkCount = WiFi.scanNetworks(false, true);
  page = 0;
  Serial.printf("Wi-Fi scan found %d networks\n", networkCount);
}

uint8_t ScannerScreen::pageCount() const {
  return networkCount > 0 ? (networkCount + networksPerPage - 1) / networksPerPage : 1;
}

void ScannerScreen::draw() {
  drawHeader("WI-FI SIGNAL SCANNER");
  if (networkCount <= 0) {
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.drawCentreString(networkCount == 0 ? "NO NETWORKS FOUND" : "SCAN FAILED", 240, 142, 2);
  } else {
    uint16_t firstNetwork = page * networksPerPage;
    uint16_t lastNetwork = min<uint16_t>(firstNetwork + networksPerPage, networkCount);
    for (uint16_t index = firstNetwork; index < lastNetwork; ++index) {
      String ssid = WiFi.SSID(index);
      if (ssid.isEmpty()) ssid = "<hidden network>";
      if (ssid.length() > 28) ssid = ssid.substring(0, 25) + "...";
      uint16_t rowY = 92 + (index - firstNetwork) * 29;
      display.setTextColor(TFT_WHITE, TFT_BLACK);
      display.drawString(ssid, 20, rowY, 2);
      int32_t rssi = WiFi.RSSI(index);
      display.setTextColor(rssi >= -67 ? TFT_GREEN : rssi >= -80 ? TFT_YELLOW : TFT_ORANGE, TFT_BLACK);
      display.drawRightString(String(rssi) + " dBm", 460, rowY, 2);
    }
    display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    display.drawCentreString("Page " + String(page + 1) + " of " + String(pageCount()), 240, 198, 2);
  }

  drawPageButton(previousButton, "PREV", page > 0);
  drawPageButton(rescanButton, "RESCAN", true);
  drawPageButton(nextButton, "NEXT", page + 1 < pageCount());
  drawBackButton();
}

void ScannerScreen::onTap(uint16_t x, uint16_t y) {
  if (previousButton.contains(x, y) && page > 0) {
    --page;
    draw();
  } else if (rescanButton.contains(x, y)) {
    scanNetworks();
    draw();
  } else if (nextButton.contains(x, y) && page + 1 < pageCount()) {
    ++page;
    draw();
  } else if (backButtonTapArea.contains(x, y)) {
    showMenu();
  }
}

void ScannerScreen::leave() {
  WiFi.scanDelete();  // free the scan results
}
