#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SD_MMC.h>
#include <TJpg_Decoder.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <Adafruit_NeoPixel.h>
#include "wifi_secrets.h"

constexpr uint8_t touchAddress = 0x38;
constexpr uint8_t touchSdaPin = 16;
constexpr uint8_t touchSclPin = 15;
constexpr uint8_t touchResetPin = 18;
constexpr uint8_t touchInterruptPin = 17;
constexpr uint8_t backlightPin = 45;
constexpr uint8_t rgbLedPin = 42;
constexpr uint8_t bootButtonPin = 0;
constexpr int sdClockPin = 38;
constexpr int sdCommandPin = 40;
constexpr int sdData0Pin = 39;
constexpr int sdData1Pin = 41;
constexpr int sdData2Pin = 48;
constexpr int sdData3Pin = 47;
constexpr unsigned long imageIntervalMs = 5000;
constexpr unsigned long menuStatusIntervalMs = 1000;
constexpr char weatherUrl[] = "https://api.open-meteo.com/v1/forecast?latitude=41.1544&longitude=-95.8900&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m&temperature_unit=fahrenheit&wind_speed_unit=mph";

TFT_eSPI display;
Adafruit_NeoPixel rgbLed(1, rgbLedPin, NEO_GRB + NEO_KHZ800);
bool lastTouch = false;
bool sdAvailable = false;
unsigned long imageShownAt = 0;
unsigned long lastMenuStatusDraw = 0;
String displayedWifiStatus;
uint8_t imageIndex = 0;
constexpr uint8_t maximumImages = 24;
String imagePaths[maximumImages];
uint8_t imageCount = 0;
constexpr uint8_t networksPerPage = 4;
int networkCount = -1;
uint8_t networkPage = 0;

enum class Screen { Menu, Weather, Images, Scanner };
Screen activeScreen = Screen::Menu;

struct WeatherData {
  bool available = false;
  String error;
  float temperature = 0;
  float windSpeed = 0;
  int humidity = 0;
  int weatherCode = 0;
} weather;

void setRgb(uint32_t color) {
  rgbLed.setPixelColor(0, color);
  rgbLed.show();
}

bool readTouch(uint16_t &x, uint16_t &y) {
  Wire.beginTransmission(touchAddress);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(touchAddress, (uint8_t)5) != 5) {
    return false;
  }

  uint8_t touches = Wire.read() & 0x0F;
  uint8_t xHigh = Wire.read();
  uint8_t xLow = Wire.read();
  uint8_t yHigh = Wire.read();
  uint8_t yLow = Wire.read();
  if (touches == 0) {
    return false;
  }

  x = ((xHigh & 0x0F) << 8) | xLow;
  y = ((yHigh & 0x0F) << 8) | yLow;
  return true;
}

void drawBackButton() {
  display.fillRect(18, 264, 444, 42, TFT_DARKGREY);
  display.drawRect(18, 264, 444, 42, TFT_LIGHTGREY);
  display.setTextColor(TFT_WHITE, TFT_DARKGREY);
  display.drawCentreString("BACK TO MENU", 240, 276, 2);
}

bool tapped(uint16_t x, uint16_t y, uint16_t left, uint16_t top, uint16_t width, uint16_t height) {
  return x >= left && x < left + width && y >= top && y < top + height;
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

void drawHeader(const char *title) {
  display.fillScreen(TFT_BLACK);
  display.setTextDatum(TL_DATUM);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.drawString("ES3C40P", 18, 16, 4);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString(title, 18, 52, 2);
  display.drawFastHLine(18, 78, 444, TFT_DARKGREY);
}

String wifiStatusText() {
  if (WiFi.status() == WL_CONNECTED) {
    return "Wi-Fi: " + String(WIFI_SSID) + "  " + WiFi.localIP().toString();
  }
  if (WiFi.status() == WL_IDLE_STATUS || WiFi.status() == WL_DISCONNECTED) {
    return "Wi-Fi: Connecting to " + String(WIFI_SSID);
  }
  return "Wi-Fi: Disconnected";
}

void drawMenuWifiStatus() {
  String status = wifiStatusText();
  display.fillRect(18, 274, 444, 30, TFT_BLACK);
  if (WiFi.status() == WL_CONNECTED) {
    display.setTextColor(TFT_GREEN, TFT_BLACK);
  } else if (WiFi.status() == WL_IDLE_STATUS || WiFi.status() == WL_DISCONNECTED) {
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
  } else {
    display.setTextColor(TFT_RED, TFT_BLACK);
  }
  display.drawString(status, 18, 282, 2);
  displayedWifiStatus = status;
}

void drawMenu() {
  drawHeader("HOME");
  display.fillRect(32, 88, 416, 42, TFT_DARKCYAN);
  display.drawRect(32, 88, 416, 42, TFT_CYAN);
  display.setTextColor(TFT_WHITE, TFT_DARKCYAN);
  display.drawString("CURRENT WEATHER", 54, 99, 2);

  display.fillRect(32, 140, 416, 42, TFT_DARKGREEN);
  display.drawRect(32, 140, 416, 42, TFT_GREEN);
  display.setTextColor(TFT_WHITE, TFT_DARKGREEN);
  display.drawString("SD IMAGE VIEWER", 54, 151, 2);

  display.fillRect(32, 192, 416, 42, TFT_DARKGREY);
  display.drawRect(32, 192, 416, 42, TFT_LIGHTGREY);
  display.setTextColor(TFT_WHITE, TFT_DARKGREY);
  display.drawString("WI-FI SIGNAL SCANNER", 54, 203, 2);

  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  display.drawString(sdAvailable ? "SD card ready" : "No SD card detected", 18, 244, 2);
  drawMenuWifiStatus();
}

void scanNetworks() {
  display.fillRect(0, 82, 480, 182, TFT_BLACK);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawCentreString("Scanning networks...", 240, 145, 2);
  networkCount = WiFi.scanNetworks(false, true);
  networkPage = 0;
  Serial.printf("Wi-Fi scan found %d networks\n", networkCount);
}

void drawScannerButton(const char *label, uint16_t left, uint16_t width, bool enabled) {
  uint16_t background = enabled ? TFT_DARKCYAN : TFT_DARKGREY;
  uint16_t foreground = enabled ? TFT_WHITE : TFT_LIGHTGREY;
  display.fillRect(left, 218, width, 32, background);
  display.drawRect(left, 218, width, 32, enabled ? TFT_CYAN : TFT_LIGHTGREY);
  display.setTextColor(foreground, background);
  display.drawCentreString(label, left + width / 2, 227, 2);
}

void drawScanner() {
  drawHeader("WI-FI SIGNAL SCANNER");
  if (networkCount <= 0) {
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.drawCentreString(networkCount == 0 ? "NO NETWORKS FOUND" : "SCAN FAILED", 240, 142, 2);
  } else {
    uint16_t firstNetwork = networkPage * networksPerPage;
    uint16_t lastNetwork = min<uint16_t>(firstNetwork + networksPerPage, networkCount);
    for (uint16_t index = firstNetwork; index < lastNetwork; ++index) {
      String ssid = WiFi.SSID(index);
      if (ssid.isEmpty()) ssid = "<hidden network>";
      if (ssid.length() > 28) ssid = ssid.substring(0, 25) + "...";
      uint16_t rowY = 92 + (index - firstNetwork) * 29;
      display.setTextColor(TFT_WHITE, TFT_BLACK);
      display.drawString(ssid, 20, rowY, 2);
      int32_t rssi = WiFi.RSSI(index);
      display.setTextColor(rssi >= -67 ? TFT_GREEN : rssi >= -80 ? TFT_YELLOW : TFT_ORANGE, TFT_BLACK);
      display.drawRightString(String(rssi) + " dBm", 460, rowY, 2);
    }
    display.setTextColor(TFT_LIGHTGREY, TFT_B
      LACK);
    display.drawCentreString("Page " + String(networkPage + 1) + " of " +
                                 String((networkCount + networksPerPage - 1) / networksPerPage),
                             240, 198, 2);
  }

  uint8_t pageCount = networkCount > 0 ? (networkCount + networksPerPage - 1) / networksPerPage : 1;
  drawScannerButton("PREV", 18, 120, networkPage > 0);
  drawScannerButton("RESCAN", 150, 180, true);
  drawScannerButton("NEXT", 342, 120, networkPage + 1 < pageCount);
  drawBackButton();
}

void drawWeather() {
  drawHeader("CURRENT WEATHER");
  if (!weather.available) {
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.drawCentreString("WEATHER UNAVAILABLE", 240, 135, 4);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawCentreString(weather.error, 240, 180, 2);
  } else {
    display.setTextColor(TFT_YELLOW, TFT_BLACK);
    display.drawCentreString(String(weather.temperature, 1) + " F", 240, 106, 7);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawCentreString(weatherDescription(weather.weatherCode), 240, 178, 4);
    display.drawCentreString("Humidity " + String(weather.humidity) + "%", 240, 220, 2);
    display.drawCentreString("Wind " + String(weather.windSpeed, 1) + " mph", 240, 244, 2);
  }
  drawBackButton();
}

bool tftOutput(int16_t x, int16_t y, uint16_t width, uint16_t height, uint16_t *bitmap) {
  if (y >= display.height()) return false;
  display.pushImage(x, y, width, height, bitmap);
  return true;
}

void scanImages() {
  imageCount = 0;
  File directory = SD_MMC.open("/");
  if (!directory) return;
  while (imageCount < maximumImages) {
    File file = directory.openNextFile();
    if (!file) break;
    String filename = file.name();
    bool isImage = !file.isDirectory() && (filename.endsWith(".jpg") || filename.endsWith(".JPG") ||
                                           filename.endsWith(".jpeg") || filename.endsWith(".JPEG"));
    if (isImage) {
      if (!filename.startsWith("/")) filename = "/" + filename;
      Serial.printf("Found JPEG: %s\n", filename.c_str());
      imagePaths[imageCount++] = filename;
    }
    file.close();
  }
  directory.close();
}

void drawImageViewer() {
  drawHeader("SD IMAGE VIEWER");
  if (!sdAvailable) {
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.drawCentreString("SD CARD NOT DETECTED", 240, 148, 4);
  } else if (imageCount == 0) {
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.drawCentreString("NO JPEG IMAGES FOUND", 240, 148, 4);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawCentreString("Place .jpg files in the card root", 240, 186, 2);
  } else {
    display.fillRect(0, 82, 480, 184, TFT_BLACK);
    uint16_t imageWidth = 0;
    uint16_t imageHeight = 0;
    File sizeFile = SD_MMC.open(imagePaths[imageIndex], FILE_READ);
    if (!sizeFile) {
      Serial.printf("Could not open JPEG: %s\n", imagePaths[imageIndex].c_str());
      display.setTextColor(TFT_ORANGE, TFT_BLACK);
      display.drawCentreString("COULD NOT OPEN JPEG", 240, 150, 2);
    } else {
      JRESULT sizeResult = TJpgDec.getSdJpgSize(&imageWidth, &imageHeight, sizeFile);
      sizeFile.close();
      if (sizeResult != JDR_OK) {
        Serial.printf("JPEG header failed for %s: %d\n", imagePaths[imageIndex].c_str(), sizeResult);
        display.setTextColor(TFT_ORANGE, TFT_BLACK);
        display.drawCentreString("JPEG FORMAT UNSUPPORTED", 240, 150, 2);
        drawBackButton();
        return;
      }
      uint8_t scale = 1;
      while ((imageWidth / scale > 480 || imageHeight / scale > 176) && scale < 8) scale *= 2;
      uint16_t scaledWidth = imageWidth / scale;
      uint16_t scaledHeight = imageHeight / scale;
      int16_t imageX = (480 - scaledWidth) / 2;
      int16_t imageY = 82 + (176 - scaledHeight) / 2;
      TJpgDec.setJpgScale(scale);
      display.setSwapBytes(true);

      File imageFile = SD_MMC.open(imagePaths[imageIndex], FILE_READ);
      JRESULT result = imageFile ? TJpgDec.drawSdJpg(imageX, imageY, imageFile) : JDR_INP;
      if (imageFile) imageFile.close();
      if (result != JDR_OK) {
        Serial.printf("JPEG decode failed for %s: %d\n", imagePaths[imageIndex].c_str(), result);
        display.setTextColor(TFT_ORANGE, TFT_BLACK);
        display.drawCentreString("JPEG DECODE FAILED", 240, 150, 2);
      }
    }
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawRightString(String(imageIndex + 1) + " / " + String(imageCount), 460, 84, 2);
  }
  drawBackButton();
}

void fetchWeather() {
  weather = {};
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi dropped; reconnecting before weather request");
    WiFi.reconnect();
    unsigned long startedAt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 10000) delay(250);
    if (WiFi.status() != WL_CONNECTED) {
      weather.error = "Could not connect to Wi-Fi";
      return;
    }
  }

  String responseBody;
  String requestError;
  for (uint8_t attempt = 1; attempt <= 2 && responseBody.isEmpty(); ++attempt) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(15000);
    HTTPClient request;
    request.setConnectTimeout(15000);
    request.setTimeout(15000);
    if (!request.begin(client, weatherUrl)) {
      requestError = "Could not start weather request";
    } else {
      int statusCode = request.GET();
      if (statusCode == HTTP_CODE_OK) {
        responseBody = request.getString();
      } else {
        requestError = HTTPClient::errorToString(statusCode);
        Serial.printf("Weather request attempt %u failed: %d (%s)\n",
                      attempt, statusCode, requestError.c_str());
      }
      request.end();
    }
    if (responseBody.isEmpty() && attempt < 2) delay(500);
  }

  if (responseBody.isEmpty()) {
    weather.error = "Weather connection failed";
    return;
  }

  JsonDocument document;
  DeserializationError error = deserializeJson(document, responseBody);
  if (error) {
    weather.error = "Could not read weather data";
    return;
  }

  JsonObject current = document["current"];
  weather.temperature = current["temperature_2m"] | 0.0;
  weather.windSpeed = current["wind_speed_10m"] | 0.0;
  weather.humidity = current["relative_humidity_2m"] | 0;
  weather.weatherCode = current["weather_code"] | 0;
  weather.available = true;
}

void showMenu() {
  activeScreen = Screen::Menu;
  drawMenu();
}

void showWeather() {
  activeScreen = Screen::Weather;
  drawHeader("CURRENT WEATHER");
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawCentreString("Loading weather...", 240, 145, 4);
  fetchWeather();
  drawWeather();
}

void showImages() {
  activeScreen = Screen::Images;
  scanImages();
  imageIndex = 0;
  imageShownAt = millis();
  drawImageViewer();
}

void showScanner() {
  activeScreen = Screen::Scanner;
  scanNetworks();
  drawScanner();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nES3C40P hardware demo");

  pinMode(backlightPin, OUTPUT);
  digitalWrite(backlightPin, HIGH);
  pinMode(bootButtonPin, INPUT_PULLUP);

  rgbLed.begin();
  rgbLed.setBrightness(32);
  setRgb(rgbLed.Color(0, 32, 32));

  pinMode(touchResetPin, OUTPUT);
  digitalWrite(touchResetPin, LOW);
  delay(10);
  digitalWrite(touchResetPin, HIGH);
  pinMode(touchInterruptPin, INPUT_PULLUP);

  Wire.begin(touchSdaPin, touchSclPin);
  Wire.setClock(100000);
  display.init();
  display.setRotation(1);
  TJpgDec.setCallback(tftOutput);
  TJpgDec.setJpgScale(1);
  SD_MMC.setPins(sdClockPin, sdCommandPin, sdData0Pin, sdData1Pin, sdData2Pin, sdData3Pin);
  sdAvailable = SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT, 5) &&
                SD_MMC.cardType() != CARD_NONE;
  Serial.printf("SD card: %s\n", sdAvailable ? "ready" : "not detected");
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  showMenu();
}

void loop() {
  uint16_t touchX;
  uint16_t touchY;
  bool touched = readTouch(touchX, touchY);
  if (touched && !lastTouch) {
    uint16_t screenX = 479 - (touchY > 479 ? 479 : touchY);
    uint16_t screenY = 319 - (touchX > 319 ? 319 : touchX);
    Serial.printf("Touch raw: x=%u y=%u, screen: x=%u y=%u\n", touchX, touchY, screenX, screenY);
    if (activeScreen == Screen::Menu && tapped(screenX, screenY, 32, 88, 416, 42)) {
      showWeather();
    } else if (activeScreen == Screen::Menu && tapped(screenX, screenY, 32, 140, 416, 42)) {
      showImages();
    } else if (activeScreen == Screen::Menu && tapped(screenX, screenY, 32, 192, 416, 42)) {
      showScanner();
    } else if (activeScreen == Screen::Scanner && tapped(screenX, screenY, 342, 218, 120, 32) && networkPage > 0) {
      --networkPage;
      drawScanner();
    } else if (activeScreen == Screen::Scanner && tapped(screenX, screenY, 150, 218, 180, 32)) {
      scanNetworks();
      drawScanner();
    } else if (activeScreen == Screen::Scanner && tapped(screenX, screenY, 18, 218, 120, 32) &&
               networkCount > 0 && (networkPage + 1) * networksPerPage < networkCount) {
      ++networkPage;
      drawScanner();
    } else if (activeScreen != Screen::Menu && tapped(screenX, screenY, 0, 252, 480, 68)) {
      if (activeScreen == Screen::Scanner) WiFi.scanDelete();
      showMenu();
    }
  }
  lastTouch = touched;

  if (activeScreen == Screen::Menu && millis() - lastMenuStatusDraw >= menuStatusIntervalMs) {
    lastMenuStatusDraw = millis();
    if (wifiStatusText() != displayedWifiStatus) drawMenuWifiStatus();
  }

  if (activeScreen == Screen::Images && imageCount > 0 && millis() - imageShownAt >= imageIntervalMs) {
    ++imageIndex;
    if (imageIndex >= imageCount) {
      showMenu();
    } else {
      imageShownAt = millis();
      drawImageViewer();
    }
  }
}
