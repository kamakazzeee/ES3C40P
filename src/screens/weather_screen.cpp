#include "screens/weather_screen.h"

#include "app.h"
#include "fonts/MontserratBold72.h"
#include "fonts/Roboto18.h"
#include "fonts/RobotoBold28.h"
#include "ui/ui.h"

WeatherScreen weatherScreen;

void WeatherScreen::enter() {
  drawHeader("CURRENT WEATHER");
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawCentreString("Loading weather...", 240, 145, 4);
  weather = fetchWeather();
}

void WeatherScreen::draw() {
  drawHeader("CURRENT WEATHER");
  if (!weather.available) {
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
