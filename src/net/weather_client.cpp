#include "net/weather_client.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <atomic>

namespace {
constexpr char weatherHost[] = "api.open-meteo.com";
constexpr char weatherUrl[] = "https://api.open-meteo.com/v1/forecast?latitude=41.1544&longitude=-95.8900&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m&temperature_unit=fahrenheit&wind_speed_unit=mph";
constexpr uint16_t connectTimeoutMs = 6000;
constexpr uint16_t responseTimeoutMs = 8000;
constexpr unsigned long handshakeTimeoutSeconds = 8;
constexpr unsigned long retryWindowMs = 8000;  // only retry a failure that came back quickly
constexpr BaseType_t weatherCore = 0;          // loop() and all drawing run on core 1

std::atomic<bool> fetchRunning(false);
WeatherData result;  // written by the fetch task only while fetchRunning is true

// Returns the response body, or an empty string after the attempts fail.
// WiFiClientSecure::setTimeout() takes seconds, unlike HTTPClient::setTimeout().
String downloadWeather() {
  String responseBody;
  unsigned long startedAt = millis();
  for (uint8_t attempt = 1; attempt <= 2 && responseBody.isEmpty(); ++attempt) {
    if (attempt == 2 && millis() - startedAt > retryWindowMs) break;
    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(handshakeTimeoutSeconds);
    client.setTimeout(responseTimeoutMs / 1000);
    HTTPClient request;
    request.setConnectTimeout(connectTimeoutMs);
    request.setTimeout(responseTimeoutMs);
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

// A failed DNS lookup is the quickest sign that the Wi-Fi network has no internet
WeatherData fetchWeather() {
  WeatherData weather;
  IPAddress address;
  if (!WiFi.hostByName(weatherHost, address)) {
    weather.error = "Wi-Fi has no internet access";
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

void weatherTask(void *) {
  result = fetchWeather();
  fetchRunning = false;
  vTaskDelete(nullptr);
}
}  // namespace

// TLS needs a larger stack than the default
void startWeatherFetch() {
  if (fetchRunning) return;
  fetchRunning = true;
  if (xTaskCreatePinnedToCore(weatherTask, "weather", 12288, nullptr, 1, nullptr, weatherCore) != pdPASS) {
    result = WeatherData();
    result.error = "Could not start weather download";
    fetchRunning = false;
  }
}

bool weatherFetchRunning() {
  return fetchRunning;
}

WeatherData latestWeather() {
  return result;
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
