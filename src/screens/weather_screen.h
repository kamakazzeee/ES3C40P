#pragma once
#include "net/weather_client.h"
#include "screens/screen.h"

class WeatherScreen : public Screen {
 public:
  void enter() override;
  void draw() override;
  void onTap(uint16_t x, uint16_t y) override;

 private:
  WeatherData weather;
};

extern WeatherScreen weatherScreen;
