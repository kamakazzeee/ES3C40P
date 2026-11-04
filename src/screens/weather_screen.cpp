#include "screens/weather_screen.h"

#include "app.h"
#include "fonts/MontserratBold72.h"
#include "fonts/Roboto18.h"
#include "fonts/RobotoBold28.h"
#include "net/wifi_connection.h"
#include "ui/ui.h"

WeatherScreen weatherScreen;

namespace {
constexpr unsigned long loadTimeoutMs = 20000;
}  // namespace

// If an earlier fetch is still running (BACK was tapped mid-load), this waits for it
void WeatherScreen::enter() {
  weather = WeatherData();
  timedOut = false;
  loadStartedAt = millis();
  if (!wifiConnected()) {
    loading = false;
    weather.error = "Not connected to Wi-Fi";
    return;
  }
  loading = true;
  startWeatherFetch();
}

// A result that arrives after the timeout message is still shown
void WeatherScreen::update() {
  if (!loading) return;
  if (!weatherFetchRunning()) {
    loading = false;
    weather = latestWeather();
    draw();
  } else if (!timedOut && millis() - loadStartedAt > loadTimeoutMs) {
    timedOut = true;
    weather.error = "No internet connection (timed out)";
    draw();
  }
}

void WeatherScreen::draw() {
  drawHeader("CURRENT WEATHER");
  if (loading && !timedOut) {
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawCentreString("Loading weather...", 240, 145, 4);
  } else if (!weather.available) {
    drawSmoothCentre(RobotoBold28, "Weather unavailable", 130, TFT_ORANGE);
    drawSmoothCentre(Roboto18, weather.error, 178, TFT_WHITE);
  } else {
    drawSmoothCentre(MontserratBold72, String(weather.temperature, 1) + "°F", 96, TFT_YELLOW);
    drawSmoothCentre(RobotoBold28, weatherDescription(weather.weatherCode), 164, TFT_WHITE);
    drawSmoothCentre(Roboto18, "Humidity " + String(weather.humidity) + "%", 204, TFT_LIGHTGREY);
    drawSmoothCentre(Roboto18, "Wind " + String(weather.windSpeed, 1) + " mph", 230, TFT_LIGHTGREY);
  }
  drawBackButton();
}

void WeatherScreen::onTap(uint16_t x, uint16_t y) {
  if (backButtonTapArea.contains(x, y)) showMenu();
}
