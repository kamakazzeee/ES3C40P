#pragma once
#include "net/weather_client.h"
#include "screens/screen.h"

class WeatherScreen : public Screen {
 public:
  void enter() override;
  void draw() override;
  void onTap(uint16_t x, uint16_t y) override;
  void update() override;

 private:
  WeatherData weather;
  bool loading = false;
  bool timedOut = false;
  unsigned long loadStartedAt = 0;
};

extern WeatherScreen weatherScreen;
