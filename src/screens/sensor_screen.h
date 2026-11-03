#pragma once
#include "screens/screen.h"

class SensorScreen : public Screen {
 public:
  void enter() override;
  void draw() override;
  void onTap(uint16_t x, uint16_t y) override;
};

extern SensorScreen sensorScreen;
