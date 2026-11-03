#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SD_MMC.h>
#include <TJpg_Decoder.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <TFT_eSPI.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_BME280.h>
#include <Adafruit_BMP280.h>
#include "wifi_secrets.h"
#include "fonts/MontserratBold72.h"
#include "fonts/Roboto18.h"
#include "fonts/RobotoBold28.h"

constexpr uint8_t touchAddress = 0x38;
constexpr uint8_t touchSdaPin = 16;
constexpr uint8_t touchSclPin = 15;
constexpr uint8_t touchResetPin = 18;
constexpr uint8_t touchInterruptPin = 17;
constexpr uint8_t backlightPin = 45;
constexpr uint8_t rgbLedPin = 42;
constexpr uint8_t bootButtonPin = 0;
constexpr uint8_t batteryAdcPin = 9;
constexpr int sdClockPin = 38;
constexpr int sdCommandPin = 40;
constexpr int sdData0Pin = 39;
constexpr int sdData1Pin = 41;
constexpr int sdData2Pin = 48;
constexpr int sdData3Pin = 47;
constexpr uint8_t codecAddress = 0x18;
constexpr uint8_t amplifierEnablePin = 1;  // FM8002E, low = on
constexpr int i2sMasterClockPin = 4;
constexpr int i2sBitClockPin = 5;
constexpr int i2sWordSelectPin = 7;
constexpr int i2sDataOutPin = 8;  // ESP32 -> ES8311 DSDIN
constexpr int i2sDataInPin = 6;   // ES8311 ASDOUT -> ESP32
constexpr uint32_t audioSampleRate = 16000;
constexpr i2s_port_t audioPort = I2S_NUM_0;
constexpr unsigned long imageIntervalMs = 5000;
constexpr unsigned long menuStatusIntervalMs = 1000;
constexpr unsigned long sensorLogIntervalMs = 300000;
constexpr unsigned long bootButtonDebounceMs = 50;
constexpr char sensorLogPath[] = "/sensor_log.csv";
constexpr char weatherUrl[] = "https://api.open-meteo.com/v1/forecast?latitude=41.1544&longitude=-95.8900&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m&temperature_unit=fahrenheit&wind_speed_unit=mph";

TFT_eSPI display;
Adafruit_NeoPixel rgbLed(1, rgbLedPin, NEO_GRB + NEO_KHZ800);
Adafruit_BME280 bme280;
Adafruit_BMP280 bmp280;
bool lastTouch = false;
bool lastBootButtonState = false;
bool screenOff = false;
bool sdAvailable = false;
bool sensorAvailable = false;
bool sensorHasHumidity = false;
bool sensorReadingValid = false;
bool audioAvailable = false;
uint8_t sensorAddress = 0;
String sensorModel;
unsigned long imageShownAt = 0;
unsigned long lastMenuStatusDraw = 0;
unsigned long lastSensorLogAt = 0;
String displayedWifiStatus;
String displayedBatteryStatus;
uint8_t imageIndex = 0;
constexpr uint8_t maximumImages = 24;
String imagePaths[maximumImages];
uint8_t imageCount = 0;
constexpr uint8_t networksPerPage = 4;
int networkCount = -1;
uint8_t networkPage = 0;

enum class Screen { Menu, Weather, Images, Scanner, Sensor };
Screen activeScreen = Screen::Menu;

struct SensorData {
  float temperature = 0;
  float pressure = 0;
  float humidity = 0;
} sensorData;

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

void drawBatteryStatus();

void drawHeader(const char *title) {
  display.fillScreen(TFT_BLACK);
  display.setTextDatum(TL_DATUM);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.drawString("ES3C40P", 18, 16, 4);
  drawBatteryStatus();
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

void drawMenuStatusBar() {
  String fullWifiStatus = wifiStatusText();
  String wifiStatus = fullWifiStatus;
  String sdStatus = sdAvailable ? "SD card ready" : "No SD card detected";
  int16_t sdLeft = 460 - display.textWidth(sdStatus, 2);
  int16_t maximumWifiWidth = sdLeft - 18 - 12;
  while (wifiStatus.length() > 3 && display.textWidth(wifiStatus + "...", 2) > maximumWifiWidth) {
    wifiStatus.remove(wifiStatus.length() - 1);
  }
  if (wifiStatus != fullWifiStatus) wifiStatus += "...";
  display.fillRect(18, 274, 444, 30, TFT_BLACK);
  if (WiFi.status() == WL_CONNECTED) {
    display.setTextColor(TFT_GREEN, TFT_BLACK);
  } else if (WiFi.status() == WL_IDLE_STATUS || WiFi.status() == WL_DISCONNECTED) {
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
  } else {
    display.setTextColor(TFT_RED, TFT_BLACK);
  }
  display.drawString(wifiStatus, 18, 282, 2);
  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.drawRightString(sdStatus, 460, 282, 2);
  displayedWifiStatus = fullWifiStatus;
}

float batteryVoltage() {
  uint32_t totalMillivolts = 0;
  constexpr uint8_t sampleCount = 16;
  for (uint8_t sample = 0; sample < sampleCount; ++sample) {
    totalMillivolts += analogReadMilliVolts(batteryAdcPin);
  }
  return (totalMillivolts / static_cast<float>(sampleCount)) * 2.0f / 1000.0f;
}

uint8_t batteryPercent(float voltage) {
 

  float minVoltage = 3.30f;
  float maxVoltage = 4.20f;

  if (voltage <= minVoltage) return 0;
  if (voltage >= maxVoltage) return 100;

  return static_cast<uint8_t>((voltage - minVoltage) / (maxVoltage - minVoltage) * 100.0f);
  
  
}

String batteryStatusText() {
  float voltage = batteryVoltage();
  uint8_t percent = batteryPercent(voltage);
  return String(percent) + "% " + String(voltage, 2) + "V";
}

void drawBatteryStatus() {
  String status = batteryStatusText();
  display.fillRect(322, 10, 140, 24, TFT_BLACK);
  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.drawRightString(status, 460, 18, 2);
  displayedBatteryStatus = status;
}

void updateSensorData() {
  if (sensorHasHumidity) {
    sensorData.temperature = bme280.readTemperature();
    sensorData.pressure = bme280.readPressure() / 100.0f;
    sensorData.humidity = bme280.readHumidity();
  } else if (sensorAvailable) {
    sensorData.temperature = bmp280.readTemperature();
    sensorData.pressure = bmp280.readPressure() / 100.0f;
  }
  sensorReadingValid = sensorAvailable && !isnan(sensorData.temperature) && !isnan(sensorData.pressure);
}

void logSensorData() {
  if (!sdAvailable) return;

  File logFile = SD_MMC.open(sensorLogPath, FILE_APPEND);
  if (!logFile) {
    Serial.printf("Could not open sensor log: %s\n", sensorLogPath);
    return;
  }

  if (logFile.size() == 0) {
    logFile.println("uptime_seconds,battery_voltage_v,battery_percent,temperature_c,temperature_f,pressure_hpa,humidity_percent,sensor_model,sensor_address");
  }

  float voltage = batteryVoltage();
  float temperatureFahrenheit = sensorData.temperature * 9.0f / 5.0f + 32.0f;
  logFile.print(millis() / 1000);
  logFile.print(",");
  logFile.print(voltage, 3);
  logFile.print(",");
  logFile.print(batteryPercent(voltage));
  logFile.print(",");
  if (sensorReadingValid) logFile.print(sensorData.temperature, 2);
  logFile.print(",");
  if (sensorReadingValid) logFile.print(temperatureFahrenheit, 2);
  logFile.print(",");
  if (sensorReadingValid) logFile.print(sensorData.pressure, 2);
  logFile.print(",");
  if (sensorHasHumidity && sensorReadingValid) logFile.print(sensorData.humidity, 2);
  logFile.print(",");
  logFile.print(sensorAvailable ? sensorModel : "");
  logFile.print(",");
  if (sensorAvailable) logFile.printf("0x%02X", sensorAddress);
  logFile.println();
  logFile.close();
  Serial.printf("Sensor data appended to %s\n", sensorLogPath);
}

void drawMenuTemperatureStatus() {
  display.fillRect(142, 10, 160, 24, TFT_BLACK);
  display.setTextColor(TFT_ORANGE, TFT_BLACK);
  if (sensorReadingValid) {
    float temperatureFahrenheit = sensorData.temperature * 9.0f / 5.0f + 32.0f;
    display.drawCentreString("Temp: " + String(temperatureFahrenheit, 1) + " F", 222, 18, 2);
  } else {
    display.drawCentreString("Temp: -- F", 222, 18, 2);
  }
}

void drawMenu() {
  drawHeader("HOME");
  drawMenuTemperatureStatus();
  display.fillRect(32, 84, 416, 40, TFT_DARKCYAN);
  display.drawRect(32, 84, 416, 40, TFT_CYAN);
  display.setTextColor(TFT_WHITE, TFT_DARKCYAN);
  display.drawString("CURRENT WEATHER", 54, 95, 2);

  display.fillRect(32, 128, 416, 40, TFT_DARKGREEN);
  display.drawRect(32, 128, 416, 40, TFT_GREEN);
  display.setTextColor(TFT_WHITE, TFT_DARKGREEN);
  display.drawString("SD IMAGE VIEWER", 54, 139, 2);

  display.fillRect(32, 172, 416, 40, TFT_DARKGREY);
  display.drawRect(32, 172, 416, 40, TFT_LIGHTGREY);
  display.setTextColor(TFT_WHITE, TFT_DARKGREY);
  display.drawString("WI-FI SIGNAL SCANNER", 54, 183, 2);

  display.fillRect(32, 216, 416, 40, TFT_DARKCYAN);
  display.drawRect(32, 216, 416, 40, TFT_CYAN);
  display.setTextColor(TFT_WHITE, TFT_DARKCYAN);
  display.drawString("ENVIRONMENT SENSOR", 54, 227, 2);

  drawMenuStatusBar();
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
    display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
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

// Smooth fonts replace the built-in ones while loaded, so unload before drawing with font numbers again
void drawSmoothCentre(const uint8_t *font, const String &text, int16_t y, uint16_t color) {
  display.loadFont(font);
  display.setTextColor(color, TFT_BLACK);
  display.setTextDatum(TC_DATUM);
  display.drawString(text, 240, y);
  display.setTextDatum(TL_DATUM);
  display.unloadFont();
}

void drawWeather() {
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

void drawSensor() {
  drawHeader("ENVIRONMENT SENSOR");
  if (!sensorAvailable) {
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.drawCentreString("SENSOR NOT DETECTED", 240, 145, 4);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawCentreString("Check the I2C connection", 240, 188, 2);
  } else if (!sensorReadingValid) {
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.drawCentreString("SENSOR READ FAILED", 240, 145, 4);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawCentreString(sensorModel + " connected at 0x" + String(sensorAddress, HEX), 240, 188, 2);
  } else {
    display.setTextColor(TFT_GREEN, TFT_BLACK);
    display.drawCentreString(sensorModel + " 0x" + String(sensorAddress, HEX), 240, 92, 2);
    display.setTextColor(TFT_YELLOW, TFT_BLACK);
    float temperatureFahrenheit = sensorData.temperature * 9.0f / 5.0f + 32.0f;
    display.drawString("Temperature: " + String(sensorData.temperature, 2) + " C / " +
                 String(temperatureFahrenheit, 2) + " F",
               28, 130, 4);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawString("Pressure: " + String(sensorData.pressure, 2) + " hPa", 28, 180, 4);
    if (sensorHasHumidity) {
      display.drawString("Humidity: " + String(sensorData.humidity, 2) + "%", 28, 230, 4);
    } else {
      display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      display.drawString("Humidity: unavailable on BMP280", 28, 230, 2);
    }
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

void showSensor() {
  activeScreen = Screen::Sensor;
  updateSensorData();
  Serial.printf("Sensor %s at 0x%02X: temperature=%.2f C, pressure=%.2f hPa\n",
                sensorModel.c_str(), sensorAddress, sensorData.temperature, sensorData.pressure);
  drawSensor();
}

void initializeSensor() {
  if (bme280.begin(0x76, &Wire)) {
    sensorAvailable = true;
    sensorHasHumidity = true;
    sensorAddress = 0x76;
    sensorModel = "BME280";
  } else if (bme280.begin(0x77, &Wire)) {
    sensorAvailable = true;
    sensorHasHumidity = true;
    sensorAddress = 0x77;
    sensorModel = "BME280";
  }
  if (sensorAvailable) {
    Serial.printf("%s sensor detected at 0x%02X\n", sensorModel.c_str(), sensorAddress);
    return;
  }
  if (bmp280.begin(0x76)) {
    sensorAvailable = true;
    sensorAddress = 0x76;
    sensorModel = "BMP280";
  } else if (bmp280.begin(0x77)) {
    sensorAvailable = true;
    sensorAddress = 0x77;
    sensorModel = "BMP280";
  }
  if (sensorAvailable) {
    Serial.printf("%s sensor detected at 0x%02X\n", sensorModel.c_str(), sensorAddress);
  } else {
    Serial.println("No BME280 or BMP280 sensor detected");
  }
}

bool writeCodecRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(codecAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

// ES8311 as I2S slave, MCLK = 256 x sample rate, 16-bit samples, DAC to speaker output
bool initializeAudio() {
  pinMode(amplifierEnablePin, OUTPUT);
  digitalWrite(amplifierEnablePin, HIGH);

  Wire.beginTransmission(codecAddress);
  if (Wire.endTransmission() != 0) {
    Serial.println("ES8311 codec not found");
    return false;
  }

  i2s_config_t config = {};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate = audioSampleRate;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.dma_buf_count = 4;
  config.dma_buf_len = 256;
  config.tx_desc_auto_clear = true;
  config.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  i2s_pin_config_t pins = {};
  pins.mck_io_num = i2sMasterClockPin;
  pins.bck_io_num = i2sBitClockPin;
  pins.ws_io_num = i2sWordSelectPin;
  pins.data_out_num = i2sDataOutPin;
  pins.data_in_num = i2sDataInPin;
  if (i2s_driver_install(audioPort, &config, 0, nullptr) != ESP_OK ||
      i2s_set_pin(audioPort, &pins) != ESP_OK) {
    Serial.println("I2S driver failed to start");
    return false;
  }

  const uint8_t setup[][2] = {
      {0x00, 0x1F}, {0x00, 0x00},  // reset
      {0x00, 0x80},                // power on, slave mode
      {0x01, 0x3F},                // all clocks on, MCLK from pin
      {0x02, 0x00},                // pre-divide 1, multiply 1
      {0x03, 0x10}, {0x04, 0x10},  // ADC and DAC oversampling
      {0x05, 0x00},                // ADC and DAC clock divide 1
      {0x06, 0x03}, {0x07, 0x00}, {0x08, 0xFF},
      {0x09, 0x0C}, {0x0A, 0x0C},  // I2S format, 16-bit in and out
      {0x0B, 0x00}, {0x0C, 0x00}, {0x10, 0x1F}, {0x11, 0x7F},
      {0x0D, 0x01},                // power up analog circuits
      {0x0E, 0x02},                // enable analog PGA and ADC modulator
      {0x12, 0x00},                // power up DAC
      {0x13, 0x10},                // enable output drive
      {0x1C, 0x6A}, {0x37, 0x08},  // bypass equalizers
      {0x32, 0xA8},                // DAC volume, 0xBF = 0 dB, 0.5 dB steps
      {0x31, 0x00},                // unmute DAC
  };
  for (const auto &entry : setup) {
    if (!writeCodecRegister(entry[0], entry[1])) {
      Serial.printf("ES8311 write to 0x%02X failed\n", entry[0]);
      return false;
    }
    if (entry[0] == 0x00 && entry[1] == 0x1F) delay(20);
  }
  Serial.println("ES8311 codec ready");
  return true;
}

// Rising C major arpeggio, each note a sine with a soft attack and exponential decay
void playStartupChime() {
  if (!audioAvailable) return;
  constexpr float notes[] = {523.25f, 659.25f, 783.99f, 1046.50f};
  constexpr float noteSeconds = 0.14f;
  constexpr float tailSeconds = 0.45f;
  constexpr float amplitude = 9000.0f;
  const uint32_t noteSamples = noteSeconds * audioSampleRate;
  const uint32_t totalSamples = noteSamples * 3 + tailSeconds * audioSampleRate;

  digitalWrite(amplifierEnablePin, LOW);
  delay(20);
  int16_t frames[256 * 2];
  uint32_t sample = 0;
  while (sample < totalSamples) {
    size_t count = 0;
    for (; count < 256 && sample < totalSamples; ++count, ++sample) {
      float value = 0;
      for (uint8_t note = 0; note < 4; ++note) {
        uint32_t start = note * noteSamples;
        if (sample < start) continue;
        float t = (sample - start) / static_cast<float>(audioSampleRate);
        float envelope = min(t / 0.005f, 1.0f) * expf(-t * (note == 3 ? 4.0f : 9.0f));
        value += envelope * sinf(2.0f * PI * notes[note] * t);
      }
      int16_t level = static_cast<int16_t>(constrain(value * amplitude, -32767.0f, 32767.0f));
      frames[count * 2] = level;
      frames[count * 2 + 1] = level;
    }
    size_t written;
    i2s_write(audioPort, frames, count * 2 * sizeof(int16_t), &written, portMAX_DELAY);
  }
  i2s_zero_dma_buffer(audioPort);
  delay(60);
  digitalWrite(amplifierEnablePin, HIGH);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nES3C40P hardware demo");

  pinMode(backlightPin, OUTPUT);
  digitalWrite(backlightPin, HIGH);
  pinMode(bootButtonPin, INPUT_PULLUP);
  lastBootButtonState = digitalRead(bootButtonPin) == LOW;
  analogSetPinAttenuation(batteryAdcPin, ADC_11db);

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
  initializeSensor();
  updateSensorData();
  audioAvailable = initializeAudio();
  display.init();
  display.setRotation(1);
  TJpgDec.setCallback(tftOutput);
  TJpgDec.setJpgScale(1);
  SD_MMC.setPins(sdClockPin, sdCommandPin, sdData0Pin, sdData1Pin, sdData2Pin, sdData3Pin);
  sdAvailable = SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT, 5) &&
                SD_MMC.cardType() != CARD_NONE;
  Serial.printf("SD card: %s\n", sdAvailable ? "ready" : "not detected");
  logSensorData();
  lastSensorLogAt = millis();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  showMenu();
  playStartupChime();
}

void loop() {
  bool bootButtonPressed = digitalRead(bootButtonPin) == LOW;
  static unsigned long lastBootButtonChangeAt = 0;
  if (bootButtonPressed != lastBootButtonState &&
      millis() - lastBootButtonChangeAt >= bootButtonDebounceMs) {
    lastBootButtonChangeAt = millis();
    lastBootButtonState = bootButtonPressed;
    if (bootButtonPressed) {
      screenOff = !screenOff;
      digitalWrite(backlightPin, screenOff ? LOW : HIGH);
    }
  }

  uint16_t touchX;
  uint16_t touchY;
  bool touched = readTouch(touchX, touchY);
  if (touched && !lastTouch) {
    uint16_t screenX = 479 - (touchY > 479 ? 479 : touchY);
    uint16_t screenY = 319 - (touchX > 319 ? 319 : touchX);
    Serial.printf("Touch raw: x=%u y=%u, screen: x=%u y=%u\n", touchX, touchY, screenX, screenY);
    if (activeScreen == Screen::Menu && tapped(screenX, screenY, 32, 84, 416, 40)) {
      showWeather();
    } else if (activeScreen == Screen::Menu && tapped(screenX, screenY, 32, 128, 416, 40)) {
      showImages();
    } else if (activeScreen == Screen::Menu && tapped(screenX, screenY, 32, 172, 416, 40)) {
      showScanner();
    } else if (activeScreen == Screen::Menu && tapped(screenX, screenY, 32, 216, 416, 40)) {
      showSensor();
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
    updateSensorData();
    drawMenuTemperatureStatus();
    if (batteryStatusText() != displayedBatteryStatus) drawBatteryStatus();
    if (wifiStatusText() != displayedWifiStatus) drawMenuStatusBar();
  }

  if (sdAvailable && millis() - lastSensorLogAt >= sensorLogIntervalMs) {
    lastSensorLogAt = millis();
    updateSensorData();
    logSensorData();
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
