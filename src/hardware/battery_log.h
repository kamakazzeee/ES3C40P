#pragma once
#include <Arduino.h>
#include <functional>

// Battery discharge runs, one CSV per run in /battery on the SD card:
//   # ES3C40P battery run 3, cell 2
//   elapsed_s,voltage_v
//   0,4.187
//   10,4.181
//   # stopped          (only if STOP was tapped; a run that just ends is one that ran flat)
// Each line is written and the file closed straight away, so the board dying loses nothing.
constexpr uint8_t maxBatteryRuns = 10;  // starting another deletes the oldest

struct BatteryRun {
  uint16_t number;  // counts up: run_<number>.csv
  uint8_t cell;     // which battery, chosen when the run started
  uint32_t seconds; // elapsed time of the last sample
  bool stopped;     // ended with STOP rather than the battery running out
};

uint8_t listBatteryRuns(BatteryRun *runs, uint8_t capacity);  // oldest first
String batteryRunPath(uint16_t number);
// Calls onSample for each logged sample, in order; false if the file can't be opened
bool readBatteryRun(uint16_t number, const std::function<void(uint32_t seconds, float voltage)> &onSample);

// Creates the next run file (deleting the oldest when there are already maxBatteryRuns); 0 on failure
uint16_t startBatteryRun(uint8_t cell);
bool logBatteryRunSample(uint16_t number, uint32_t seconds, float voltage);
void stopBatteryRun(uint16_t number);
