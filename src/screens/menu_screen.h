#pragma once
#include "screens/screen.h"

// Home screen: a 3 x 2 grid of tiles per page; swiping left or right slides to the next page
class MenuScreen : public Screen {
 public:
  void draw() override;
  void onTap(uint16_t x, uint16_t y) override;
  bool usesSwipes() const override { return true; }
  void onSwipe(int8_t direction) override;
  void update() override;

 private:
  void slideToPage(uint8_t newPage, int8_t direction);
  void drawPageDots();
  void drawTemperatureStatus();
  void drawStatusBar();
  uint8_t page = 0;
  unsigned long lastStatusDrawAt = 0;
  String displayedWifiStatus;
  bool offeredWifiScreen = false;  // opens by itself only once per boot, so BACK sticks
};

extern MenuScreen menuScreen;
