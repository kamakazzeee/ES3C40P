#pragma once
#include <Arduino.h>

// The simple on-board parts: backlight, BOOT button and RGB LED
void beginBoard();
void setBacklight(bool on);
bool backlightOn();
bool bootButtonPressed();  // true once per press, debounced
void setRgb(uint8_t red, uint8_t green, uint8_t blue);
