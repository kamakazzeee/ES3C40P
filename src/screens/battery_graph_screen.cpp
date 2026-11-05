#include "screens/battery_graph_screen.h"

#include "app.h"
#include "hardware/battery_log.h"
#include "hardware/sd_storage.h"
#include "ui/ui.h"

BatteryGraphScreen batteryGraphScreen;

namespace {
constexpr Button plot = {46, 92, 300, 146};
constexpr float lowestVolts = 3.0f;
constexpr float highestVolts = 4.3f;
constexpr int16_t legendLeft = 358;
constexpr uint16_t runColours[maxBatteryRuns] = {TFT_CYAN,   TFT_YELLOW, TFT_GREEN,  TFT_MAGENTA,     TFT_ORANGE,
                                                 TFT_RED,    TFT_SKYBLUE, TFT_PINK,  TFT_GREENYELLOW, TFT_WHITE};

int16_t voltsToY(float volts) {
  volts = constrain(volts, lowestVolts, highestVolts);
  return plot.top + plot.height - 1 - int16_t((volts - lowestVolts) / (highestVolts - lowestVolts) * (plot.height - 1));
}

String formatDuration(uint32_t seconds) {
  char text[12];
  snprintf(text, sizeof(text), "%luh%02lu", (unsigned long)(seconds / 3600), (unsigned long)(seconds / 60 % 60));
  return text;
}

void drawAxes(uint8_t hours) {
  for (int tenths = 30; tenths <= 42; tenths += 2) {
    int16_t y = voltsToY(tenths / 10.0f);
    display.drawFastHLine(plot.left, y, plot.width, tenths % 4 == 2 ? TFT_DARKGREY : 0x2104);
    if (tenths % 4 == 2) {  // label 3.0, 3.4, 3.8, 4.2
      display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      display.drawRightString(String(tenths / 10.0f, 1), plot.left - 6, y - 7, 2);
    }
  }
  uint8_t step = hours > 8 ? 2 : 1;
  for (uint8_t hour = 0; hour <= hours; hour += step) {
    int16_t x = plot.left + int32_t(hour) * (plot.width - 1) / hours;
    display.drawFastVLine(x, plot.top, plot.height, hour == 0 ? TFT_DARKGREY : 0x2104);
    display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    display.drawCentreString(String(hour) + "h", x, plot.top + plot.height + 3, 2);
  }
  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  display.drawString("V", 18, plot.top - 4, 2);
}

void drawRun(const BatteryRun &run, uint8_t hours, uint16_t colour) {
  uint32_t span = uint32_t(hours) * 3600;
  int16_t lastX = -1;
  int16_t lastY = 0;
  readBatteryRun(run.number, [&](uint32_t seconds, float volts) {
    int16_t x = plot.left + int32_t(min(seconds, span) * uint64_t(plot.width - 1) / span);
    int16_t y = voltsToY(volts);
    if (lastX < 0) {
      display.drawPixel(x, y, colour);
    } else if (x != lastX) {  // one point per pixel column is plenty
      display.drawLine(lastX, lastY, x, y, colour);
    } else {
      return;
    }
    lastX = x;
    lastY = y;
  });
}

void drawLegend(const BatteryRun *runs, uint8_t count) {
  for (uint8_t i = 0; i < count; ++i) {
    int16_t y = plot.top - 2 + i * 15;
    display.fillRect(legendLeft, y + 5, 10, 5, runColours[i]);
    display.setTextColor(runs[i].stopped ? TFT_DARKGREY : TFT_WHITE, TFT_BLACK);
    String label = "#" + String(runs[i].number) + " c" + String(runs[i].cell) + " " +
                   (runs[i].stopped ? String("stopped") : formatDuration(runs[i].seconds));
    display.drawString(label, legendLeft + 14, y, 2);
  }
}

void drawMessage(const char *title, const char *detail) {
  display.setTextColor(TFT_ORANGE, TFT_BLACK);
  display.drawCentreString(title, 240, 140, 4);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawCentreString(detail, 240, 182, 2);
}
}  // namespace

void BatteryGraphScreen::draw() {
  drawHeader("BATTERY CURVES");
  drawBackButton();
  if (!sdCardReady()) {
    drawMessage("NO SD CARD", "Battery runs are stored on the SD card");
    return;
  }
  BatteryRun runs[maxBatteryRuns];
  uint8_t count = listBatteryRuns(runs, maxBatteryRuns);
  if (!count) {
    drawMessage("NO BATTERY RUNS YET", "Record one with the Battery Test tile");
    return;
  }

  uint32_t longest = 0;
  for (uint8_t i = 0; i < count; ++i) longest = max(longest, runs[i].seconds);
  uint8_t hours = max<uint32_t>(1, (longest + 3599) / 3600);
  drawAxes(hours);
  drawLegend(runs, count);
  for (uint8_t i = 0; i < count; ++i) drawRun(runs[i], hours, runColours[i]);
}

void BatteryGraphScreen::onTap(uint16_t x, uint16_t y) {
  if (backButtonTapArea.contains(x, y)) showMenu();
}
