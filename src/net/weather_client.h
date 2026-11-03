#pragma once
#include <Arduino.h>

// Current conditions from Open-Meteo. Knows HTTP and JSON, nothing about the screen.
struct WeatherData {
  bool available = false;
  String error;
  float temperature = 0;  // F
  float windSpeed = 0;    // mph
  int humidity = 0;       // %
  int weatherCode = 0;
};

WeatherData fetchWeather();  // blocks for up to ~40 s on a bad connection
const char *weatherDescription(int code);
