#include "screens/battery_test_screen.h"

#include "app.h"
#include "hardware/battery.h"
#include "hardware/battery_log.h"
#include "hardware/sd_storage.h"
#include "ui/ui.h"

BatteryTestScreen batteryTestScreen;

namespace {
constexpr uint8_t cellCount = 4;
constexpr uint32_t sampleIntervalSeconds = 10;
constexpr unsigned long readingIntervalMs = 1000;  // each logged sample averages 10 of these
constexpr unsigned long stopConfirmMs = 3000;
constexpr Button startButton = {18, 208, 444, 42};

Button cellButton(uint8_t index) {
  return {int16_t(90 + index * 60), 88, 50, 36};
}

String formatElapsed(uint32_t seconds) {
  char text[16];
  snprintf(text, sizeof(text), "%lu:%02lu:%02lu", (unsigned long)(seconds / 3600),
           (unsigned long)(seconds / 60 % 60), (unsigned long)(seconds % 60));
  return text;
}
}  // namespace

void BatteryTestScreen::draw() {
  drawHeader("BATTERY TEST");
  if (runNumber) {
    drawRunning();
  } else {
    drawIdle();
  }
}

void BatteryTestScreen::drawIdle() {
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString("Cell", 28, 98, 2);
  for (uint8_t index = 0; index < cellCount; ++index) {
    bool chosen = index + 1 == cell;
    drawButton(cellButton(index), String(index + 1).c_str(), chosen ? TFT_DARKGREEN : TFT_DARKGREY,
               chosen ? TFT_GREEN : TFT_LIGHTGREY, TFT_WHITE);
  }

  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString("Charge fully on USB, unplug USB, then tap START.", 18, 134, 2);
  display.drawString("Leave it on this screen until the board switches off.", 18, 154, 2);

  BatteryRun runs[maxBatteryRuns];
  uint8_t runCount = listBatteryRuns(runs, maxBatteryRuns);
  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  String saved = "Runs saved: " + String(runCount) + " of " + String(maxBatteryRuns);
  if (runCount >= maxBatteryRuns) saved += "   (START deletes run #" + String(runs[0].number) + ", the oldest)";
  display.drawString(saved, 18, 180, 2);

  if (!sdCardReady()) {
    drawButton(startButton, "NO SD CARD", TFT_DARKGREY, TFT_LIGHTGREY, TFT_LIGHTGREY);
  } else if (!batteryConnected()) {
    drawButton(startButton, "NO BATTERY DETECTED", TFT_DARKGREY, TFT_LIGHTGREY, TFT_LIGHTGREY);
  } else {
    drawButton(startButton, "START", TFT_DARKGREEN, TFT_GREEN, TFT_WHITE);
  }
  drawBackButton();
}

void BatteryTestScreen::drawRunning() {
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.drawString("Run #" + String(runNumber) + ", cell " + String(cell) + "   " + batteryRunPath(runNumber), 18, 88,
                     2);
  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  display.drawString("Leave it until the board switches off; keep the screen on.", 18, 228, 2);
  drawRunningValues();
  drawStopButton();
}

void BatteryTestScreen::drawRunningValues() {
  uint32_t elapsed = (millis() - startedAt) / 1000;
  display.setTextPadding(420);  // overwrites the old text in place, without a flickering clear
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString("Elapsed  " + formatElapsed(elapsed), 28, 112, 4);
  display.drawString("Voltage  " + String(lastVoltage, 3) + " V", 28, 146, 4);
  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  display.drawString("Samples logged: " + String(samplesLogged) + "    Level now: " +
                         String(batteryPercent(lastVoltage)) + "%",
                     28, 182, 2);
  display.setTextPadding(0);
  if (writeFailed) {
    display.setTextColor(TFT_RED, TFT_BLACK);
    display.drawString("SD WRITE FAILED - check the card", 28, 202, 2);
  }
}

void BatteryTestScreen::drawStopButton() {
  if (stopArmedAt) {
    drawButton(backButton, "TAP AGAIN TO STOP", TFT_RED, TFT_WHITE, TFT_WHITE);
  } else {
    drawButton(backButton, "STOP TEST", TFT_MAROON, TFT_RED, TFT_WHITE);
  }
}

void BatteryTestScreen::onTap(uint16_t x, uint16_t y) {
  if (runNumber) {
    if (!backButtonTapArea.contains(x, y)) return;
    if (stopArmedAt) {
      stop();
    } else {
      stopArmedAt = millis();
      drawStopButton();
    }
    return;
  }
  for (uint8_t index = 0; index < cellCount; ++index) {
    if (cellButton(index).contains(x, y)) {
      cell = index + 1;
      draw();
      return;
    }
  }
  if (startButton.contains(x, y) && sdCardReady() && batteryConnected()) {
    start();
  } else if (backButtonTapArea.contains(x, y)) {
    showMenu();
  }
}

void BatteryTestScreen::start() {
  runNumber = startBatteryRun(cell);
  if (!runNumber) {
    display.setTextColor(TFT_RED, TFT_BLACK);
    display.drawString("Could not create the run file", 18, 180, 2);
    return;
  }
  startedAt = millis();
  lastReadingAt = startedAt;
  lastVoltage = batteryVoltage();
  writeFailed = !logBatteryRunSample(runNumber, 0, lastVoltage);
  samplesLogged = writeFailed ? 0 : 1;
  nextSampleSeconds = sampleIntervalSeconds;
  voltageTotal = 0;
  voltageReadings = 0;
  stopArmedAt = 0;
  draw();
}

void BatteryTestScreen::stop() {
  stopBatteryRun(runNumber);
  runNumber = 0;
  stopArmedAt = 0;
  draw();
}

void BatteryTestScreen::update() {
  if (!runNumber) return;
  if (stopArmedAt && millis() - stopArmedAt > stopConfirmMs) {
    stopArmedAt = 0;
    drawStopButton();
  }
  if (millis() - lastReadingAt < readingIntervalMs) return;
  lastReadingAt += readingIntervalMs;
  lastVoltage = batteryVoltage();
  voltageTotal += lastVoltage;
  ++voltageReadings;

  uint32_t elapsed = (millis() - startedAt) / 1000;
  if (elapsed >= nextSampleSeconds && voltageReadings) {
    if (logBatteryRunSample(runNumber, nextSampleSeconds, voltageTotal / voltageReadings)) {
      ++samplesLogged;
    } else {
      writeFailed = true;
    }
    nextSampleSeconds += sampleIntervalSeconds;
    voltageTotal = 0;
    voltageReadings = 0;
  }
  drawRunningValues();
}

// Nothing leaves this screen mid-run (BACK is replaced by STOP), but don't leave a run open if it happens
void BatteryTestScreen::leave() {
  if (!runNumber) return;
  stopBatteryRun(runNumber);
  runNumber = 0;
}
