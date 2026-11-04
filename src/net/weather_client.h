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

// The download runs in a background task so the screen stays usable on a network with
// no internet. startWeatherFetch() does nothing while a fetch is already running.
void startWeatherFetch();
bool weatherFetchRunning();
WeatherData latestWeather();  // last finished fetch; only read it while no fetch is running
const char *weatherDescription(int code);
