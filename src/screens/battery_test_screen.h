#pragma once
#include "screens/screen.h"

// Logs the battery voltage every 10 s to /battery/run_<n>.csv until the board runs flat, so a
// discharge curve can be measured on this board's own load. Pick the cell number, unplug USB, START.
class BatteryTestScreen : public Screen {
 public:
  void draw() override;
  void onTap(uint16_t x, uint16_t y) override;
  void update() override;
  void leave() override;

 private:
  void drawIdle();
  void drawRunning();
  void drawRunningValues();
  void drawStopButton();
  void start();
  void stop();
  uint8_t cell = 1;
  uint16_t runNumber = 0;  // 0 while no run is going
  unsigned long startedAt = 0;
  unsigned long lastReadingAt = 0;
  uint32_t nextSampleSeconds = 0;
  float voltageTotal = 0;
  uint8_t voltageReadings = 0;
  float lastVoltage = 0;
  uint32_t samplesLogged = 0;
  bool writeFailed = false;
  unsigned long stopArmedAt = 0;  // STOP needs a second tap within 3 s
};

extern BatteryTestScreen batteryTestScreen;
