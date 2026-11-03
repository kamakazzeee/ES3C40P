#include "hardware/board.h"

#include <Adafruit_NeoPixel.h>

#include "pins.h"

namespace {
constexpr unsigned long bootButtonDebounceMs = 50;

Adafruit_NeoPixel rgbLed(1, rgbLedPin, NEO_GRB + NEO_KHZ800);
bool isBacklightOn = true;
bool lastBootButtonState = false;
unsigned long lastBootButtonChangeAt = 0;
}  // namespace

void beginBoard() {
  pinMode(backlightPin, OUTPUT);
  setBacklight(true);
  pinMode(bootButtonPin, INPUT_PULLUP);
  lastBootButtonState = digitalRead(bootButtonPin) == LOW;
  rgbLed.begin();
  rgbLed.setBrightness(32);
}

void setBacklight(bool on) {
  isBacklightOn = on;
  digitalWrite(backlightPin, on ? HIGH : LOW);
}

bool backlightOn() {
  return isBacklightOn;
}

bool bootButtonPressed() {
  bool pressed = digitalRead(bootButtonPin) == LOW;
  if (pressed == lastBootButtonState || millis() - lastBootButtonChangeAt < bootButtonDebounceMs) {
    return false;
  }
  lastBootButtonChangeAt = millis();
  lastBootButtonState = pressed;
  return pressed;
}

void setRgb(uint8_t red, uint8_t green, uint8_t blue) {
  rgbLed.setPixelColor(0, rgbLed.Color(red, green, blue));
  rgbLed.show();
}
