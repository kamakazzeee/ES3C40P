#include "screens/menu_screen.h"

#include "app.h"
#include "hardware/env_sensor.h"
#include "net/wifi_connection.h"
#include "screens/battery_graph_screen.h"
#include "screens/battery_test_screen.h"
#include "screens/image_screen.h"
#include "screens/mic_screen.h"
#include "screens/scanner_screen.h"
#include "screens/sensor_screen.h"
#include "screens/weather_screen.h"
#include "screens/wifi_screen.h"
#include "ui/icons.h"
#include "ui/ui.h"

MenuScreen menuScreen;

namespace {
constexpr unsigned long temperatureIntervalMs = 1000;
constexpr Button tileArea = {0, 50, 480, 256};  // between the header line and the page dots
constexpr uint8_t tilesPerPage = 6;
constexpr int16_t pageDotsY = 311;
constexpr uint8_t slideFrames = 8;
constexpr uint16_t darkOrange = 0x7A80;

struct MenuItem {
  const char *label;
  IconDrawer drawIcon;
  uint16_t fill;
  uint16_t border;
  Screen *screen;
};

// To add a menu option: write a new screen and add one line here. Every six make a new page.
const MenuItem menuItems[] = {
    {"WEATHER", drawWeatherIcon, TFT_DARKCYAN, TFT_CYAN, &weatherScreen},
    {"PHOTOS", drawPhotoIcon, TFT_DARKGREEN, TFT_GREEN, &imageScreen},
    {"WI-FI SCANNER", drawSignalIcon, TFT_DARKGREY, TFT_LIGHTGREY, &scannerScreen},
    {"SENSOR", drawThermometerIcon, darkOrange, TFT_ORANGE, &sensorScreen},
    {"MICROPHONE", drawMicrophoneIcon, TFT_MAROON, TFT_RED, &micScreen},
    {"WI-FI", drawWifiIcon, TFT_NAVY, TFT_SKYBLUE, &wifiScreen},
    {"BATTERY TEST", drawBatteryTestIcon, TFT_OLIVE, TFT_GREENYELLOW, &batteryTestScreen},
    {"BATTERY CURVES", drawGraphIcon, TFT_PURPLE, TFT_MAGENTA, &batteryGraphScreen},
};
constexpr uint8_t itemCount = sizeof(menuItems) / sizeof(menuItems[0]);
constexpr uint8_t pageCount = (itemCount + tilesPerPage - 1) / tilesPerPage;

// Where a slot on the page sits on screen; used for both drawing and tap tests
Button tileButton(uint8_t slot) {
  return {int16_t(18 + (slot % 3) * 152), int16_t(56 + (slot / 3) * 128), 140, 118};
}

// Draws one page of tiles, shifted by (offsetX, offsetY) so it can go onto a sprite as well as the screen
void drawTiles(TFT_eSPI &canvas, uint8_t page, int16_t offsetX, int16_t offsetY) {
  for (uint8_t slot = 0; slot < tilesPerPage; ++slot) {
    uint8_t index = page * tilesPerPage + slot;
    if (index >= itemCount) break;
    const MenuItem &item = menuItems[index];
    Button tile = tileButton(slot);
    int16_t left = tile.left + offsetX;
    int16_t top = tile.top + offsetY;
    int16_t centreX = left + tile.width / 2;
    canvas.fillSmoothRoundRect(left, top, tile.width, tile.height, 8, item.border, TFT_BLACK);
    canvas.fillSmoothRoundRect(left + 2, top + 2, tile.width - 4, tile.height - 4, 6, item.fill, item.border);
    item.drawIcon(canvas, centreX, top + 46, item.fill);
    canvas.setTextColor(TFT_WHITE, item.fill);
    canvas.drawCentreString(item.label, centreX, top + 86, 2);
  }
}
}  // namespace

void MenuScreen::draw() {
  drawHeader(nullptr);
  drawTemperatureStatus();
  drawTiles(display, page, 0, 0);
  drawPageDots();
}

void MenuScreen::onTap(uint16_t x, uint16_t y) {
  for (uint8_t slot = 0; slot < tilesPerPage; ++slot) {
    uint8_t index = page * tilesPerPage + slot;
    if (index < itemCount && tileButton(slot).contains(x, y)) {
      showScreen(*menuItems[index].screen);
      return;
    }
  }
}

// Swiping left shows the next page and swiping right the previous one; both wrap around
void MenuScreen::onSwipe(int8_t direction) {
  if (pageCount < 2) return;
  uint8_t newPage = direction < 0 ? (page + 1) % pageCount : (page + pageCount - 1) % pageCount;
  slideToPage(newPage, direction);
}

// Both pages go side by side into one sprite in PSRAM (960 wide), which is pushed a little
// further across each frame, so the tiles move without being redrawn or flickering (~33 ms a frame)
void MenuScreen::slideToPage(uint8_t newPage, int8_t direction) {
  TFT_eSprite strip(&display);
  strip.setColorDepth(16);
  if (!strip.createSprite(2 * tileArea.width, tileArea.height)) {
    page = newPage;
    display.fillRect(tileArea.left, tileArea.top, tileArea.width, tileArea.height, TFT_BLACK);
    drawTiles(display, page, 0, 0);
    drawPageDots();
    return;
  }
  strip.fillSprite(TFT_BLACK);
  int16_t oldPageX = direction < 0 ? 0 : tileArea.width;  // finger moving left: new page comes in from the right
  drawTiles(strip, page, oldPageX, -tileArea.top);
  drawTiles(strip, newPage, tileArea.width - oldPageX, -tileArea.top);
  for (uint8_t frame = 1; frame <= slideFrames; ++frame) {
    float progress = float(frame) / slideFrames;
    int16_t shift = tileArea.width * (1 - (1 - progress) * (1 - progress));  // eases out
    strip.pushSprite(direction < 0 ? -shift : shift - tileArea.width, tileArea.top);
  }
  strip.deleteSprite();
  page = newPage;
  drawPageDots();
}

void MenuScreen::drawPageDots() {
  display.fillRect(0, pageDotsY - 4, 480, 9, TFT_BLACK);
  if (pageCount < 2) return;
  int16_t firstX = 240 - (pageCount - 1) * 7;
  for (uint8_t dot = 0; dot < pageCount; ++dot) {
    display.fillSmoothCircle(firstX + dot * 14, pageDotsY, 3, dot == page ? TFT_WHITE : TFT_DARKGREY, TFT_BLACK);
  }
}

void MenuScreen::update() {
  if (millis() - lastTemperatureDrawAt < temperatureIntervalMs) return;
  lastTemperatureDrawAt = millis();
  updateEnvSensor();
  drawTemperatureStatus();
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
