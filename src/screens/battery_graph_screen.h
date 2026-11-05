#pragma once
#include "screens/screen.h"

// Plots voltage against time for every saved battery run (up to 10), one colour each
class BatteryGraphScreen : public Screen {
 public:
  void draw() override;
  void onTap(uint16_t x, uint16_t y) override;
};

extern BatteryGraphScreen batteryGraphScreen;
