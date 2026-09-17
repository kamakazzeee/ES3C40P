#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <Adafruit_NeoPixel.h>

constexpr uint8_t touchAddress = 0x38;
constexpr uint8_t touchSdaPin = 16;
constexpr uint8_t touchSclPin = 15;
constexpr uint8_t touchResetPin = 18;
constexpr uint8_t touchInterruptPin = 17;
constexpr uint8_t backlightPin = 45;
constexpr uint8_t rgbLedPin = 42;
constexpr uint8_t bootButtonPin = 0;
constexpr unsigned long refreshIntervalMs = 500;
constexpr unsigned long scanIntervalMs = 15000;
TFT_eSPI display;
Adafruit_NeoPixel rgbLed(1, rgbLedPin, NEO_GRB + NEO_KHZ800);
unsigned long lastRefresh = 0;
unsigned long lastScan = scanIntervalMs;
int networkCount = 0;
uint8_t colorIndex = 0;
bool lastTouch = false;

const uint16_t colors[] = {TFT_CYAN, TFT_GREEN, TFT_ORANGE, TFT_MAGENTA};

void setRgb(uint32_t color) {
  rgbLed.setPixelColor(0, color);
  rgbLed.show();
}

void scanNetworks() {
  Serial.println("\nWi-Fi scan");
  networkCount = WiFi.scanNetworks(false, true);

  if (networkCount <= 0) {
    Serial.println("No networks found.");
  } else {
    for (int index = 0; index < networkCount; ++index) {
      Serial.printf("%2d: %-32s %4d dBm %s\n",
                    index + 1,
                    WiFi.SSID(index).c_str(),
                    WiFi.RSSI(index),
                    WiFi.encryptionType(index) == WIFI_AUTH_OPEN ? "open" : "secured");
    }
  }

  WiFi.scanDelete();
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

void drawDashboard() {
  display.fillScreen(TFT_BLACK);
  display.setTextDatum(TL_DATUM);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.drawString("ES3C40P", 18, 16, 4);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString("ESP32-S3 hardware demo", 18, 52, 2);
  display.drawFastHLine(18, 78, 444, TFT_DARKGREY);

  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.drawString("Display", 18, 98, 2);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString("ST7796S  480 x 320", 18, 122, 2);
  display.drawString("Touch", 18, 154, 2);
  display.drawString("FT6336U  I2C 0x38", 18, 178, 2);
  display.drawString("Wi-Fi networks", 18, 210, 2);
  display.drawNumber(networkCount, 18, 234, 4);

  display.setTextColor(colors[colorIndex], TFT_BLACK);
  display.drawString("Tap screen to change LED color", 270, 100, 2);
  display.fillCircle(390, 190, 48, colors[colorIndex]);
  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  display.drawString("BOOT button: GPIO0", 270, 250, 2);
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
  display.init();
  display.setRotation(1);
  drawDashboard();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  scanNetworks();
}

void loop() {
  unsigned long now = millis();

  uint16_t touchX;
  uint16_t touchY;
  bool touched = readTouch(touchX, touchY);
  if (touched && !lastTouch) {
    colorIndex = (colorIndex + 1) % (sizeof(colors) / sizeof(colors[0]));
    setRgb(rgbLed.Color((colorIndex + 1) * 16, 32 - colorIndex * 6, 24));
    Serial.printf("Touch: x=%u y=%u\n", touchX, touchY);
    drawDashboard();
  }
  lastTouch = touched;

  if (now - lastRefresh >= refreshIntervalMs) {
    lastRefresh = now;
    display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    display.drawString("Uptime: " + String(now / 1000) + " s", 18, 282, 2);
  }

  if (now - lastScan >= scanIntervalMs) {
    lastScan = now;
    scanNetworks();
    drawDashboard();
  }
}
