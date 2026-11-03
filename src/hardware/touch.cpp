#include "hardware/touch.h"

#include <Wire.h>

#include "pins.h"

namespace {
constexpr unsigned long touchReleaseMs = 40;  // touch controller pulses every ~10 ms while touched

volatile bool touchDataReady = false;
unsigned long lastTouchPulseAt = 0;
bool lastTouch = false;

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

bool readTap(uint16_t &x, uint16_t &y) {
  // Only talk to the touch controller after it signals new data; when the pulses stop, the finger has lifted
  uint16_t touchX = 0;
  uint16_t touchY = 0;
  bool touched = lastTouch;
  if (touchDataReady) {
    touchDataReady = false;
    lastTouchPulseAt = millis();
    touched = readTouchPoint(touchX, touchY);
  } else if (lastTouch && millis() - lastTouchPulseAt > touchReleaseMs) {
    touched = false;
  }

  bool newTap = touched && !lastTouch;
  lastTouch = touched;
  if (!newTap) return false;

  // Touch Y runs left to right across the rotated screen; X runs bottom to top
  x = touchY > 479 ? 479 : touchY;
  y = 319 - (touchX > 319 ? 319 : touchX);
  Serial.printf("Touch raw: x=%u y=%u, screen: x=%u y=%u\n", touchX, touchY, x, y);
  return true;
}
