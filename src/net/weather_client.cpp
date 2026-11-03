#include "net/weather_client.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "net/wifi_connection.h"

namespace {
constexpr char weatherUrl[] = "https://api.open-meteo.com/v1/forecast?latitude=41.1544&longitude=-95.8900&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m&temperature_unit=fahrenheit&wind_speed_unit=mph";

// Returns the response body, or an empty string after two failed attempts
String downloadWeather() {
  String responseBody;
  for (uint8_t attempt = 1; attempt <= 2 && responseBody.isEmpty(); ++attempt) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(15000);
    HTTPClient request;
    request.setConnectTimeout(15000);
    request.setTimeout(15000);
    if (!request.begin(client, weatherUrl)) {
      Serial.println("Could not start weather request");
    } else {
      int statusCode = request.GET();
      if (statusCode == HTTP_CODE_OK) {
        responseBody = request.getString();
      } else {
        Serial.printf("Weather request attempt %u failed: %d (%s)\n",
                      attempt, statusCode, HTTPClient::errorToString(statusCode).c_str());
      }
      request.end();
    }
    if (responseBody.isEmpty() && attempt < 2) delay(500);
  }
  return responseBody;
}
}  // namespace

WeatherData fetchWeather() {
  WeatherData weather;
  if (!waitForWifi(10000)) {
    weather.error = "Could not connect to Wi-Fi";
    return weather;
  }

  String responseBody = downloadWeather();
  if (responseBody.isEmpty()) {
    weather.error = "Weather connection failed";
    return weather;
  }

  JsonDocument document;
  if (deserializeJson(document, responseBody)) {
    weather.error = "Could not read weather data";
    return weather;
  }

  JsonObject current = document["current"];
  weather.temperature = current["temperature_2m"] | 0.0;
  weather.windSpeed = current["wind_speed_10m"] | 0.0;
  weather.humidity = current["relative_humidity_2m"] | 0;
  weather.weatherCode = current["weather_code"] | 0;
  weather.available = true;
  return weather;
}

const char *weatherDescription(int code) {
  if (code == 0) return "Clear sky";
  if (code <= 3) return "Partly cloudy";
  if (code <= 48) return "Foggy";
  if (code <= 57) return "Drizzle";
  if (code <= 67) return "Rain";
  if (code <= 77) return "Snow";
  if (code <= 82) return "Rain showers";
  if (code <= 86) return "Snow showers";
  return "Thunderstorm";
}
