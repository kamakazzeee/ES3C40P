#pragma once
#include "screens/screen.h"

class MenuScreen : public Screen {
 public:
  void draw() override;
  void onTap(uint16_t x, uint16_t y) override;
  void update() override;

 private:
  void drawTemperatureStatus();
  void drawStatusBar();
  unsigned long lastStatusDrawAt = 0;
  String displayedWifiStatus;
  bool offeredWifiScreen = false;  // opens by itself only once per boot, so BACK sticks
};

extern MenuScreen menuScreen;
