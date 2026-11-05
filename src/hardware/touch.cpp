#include "hardware/touch.h"

#include <Wire.h>

#include "pins.h"

namespace {
constexpr unsigned long touchReleaseMs = 40;  // touch controller pulses every ~10 ms while touched

volatile bool touchDataReady = false;
unsigned long lastTouchPulseAt = 0;
bool lastTouch = false;
uint16_t lastScreenX = 0;
uint16_t lastScreenY = 0;

// Runs on each falling edge of the touch INT pin, which pulses about every 10 ms while
// the screen is touched and stays quiet otherwise. Interrupt handlers must be short, so
// it only raises a flag and readTap() does the I2C read.
void IRAM_ATTR onTouchInterrupt() {
  touchDataReady = true;
}

bool readTouchPoint(uint16_t &x, uint16_t &y) {
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
}  // namespace

void beginTouch() {
  pinMode(touchResetPin, OUTPUT);
  digitalWrite(touchResetPin, LOW);
  delay(10);
  digitalWrite(touchResetPin, HIGH);
  pinMode(touchInterruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(touchInterruptPin), onTouchInterrupt, FALLING);
}

TouchEvent readTouchEvent(uint16_t &x, uint16_t &y) {
  // Only talk to the touch controller after it signals new data; when the pulses stop, the finger has lifted
  uint16_t touchX = 0;
  uint16_t touchY = 0;
  bool touched = lastTouch;
  if (touchDataReady) {
    touchDataReady = false;
    lastTouchPulseAt = millis();
    touched = readTouchPoint(touchX, touchY);
    if (touched) {
      // Touch Y runs left to right across the rotated screen; X runs bottom to top
      lastScreenX = touchY > 479 ? 479 : touchY;
      lastScreenY = 319 - (touchX > 319 ? 319 : touchX);
    }
  } else if (lastTouch && millis() - lastTouchPulseAt > touchReleaseMs) {
    touched = false;
  }

  TouchEvent event = TouchEvent::none;
  if (touched && !lastTouch) {
    event = TouchEvent::press;
    Serial.printf("Touch raw: x=%u y=%u, screen: x=%u y=%u\n", touchX, touchY, lastScreenX, lastScreenY);
  } else if (!touched && lastTouch) {
    event = TouchEvent::release;
  }
  lastTouch = touched;
  x = lastScreenX;
  y = lastScreenY;
  return event;
}

bool readTap(uint16_t &x, uint16_t &y) {
  return readTouchEvent(x, y) == TouchEvent::press;
}
