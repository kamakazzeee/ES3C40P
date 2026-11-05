#pragma once
#include <Arduino.h>

// FT6336U capacitive touch controller
void beginTouch();

enum class TouchEvent : uint8_t { none, press, release };

// press: where a finger landed; release: the last place it was seen before lifting.
// x and y are in screen pixels (480 x 320).
TouchEvent readTouchEvent(uint16_t &x, uint16_t &y);

// True once when a finger first lands; releases are skipped
bool readTap(uint16_t &x, uint16_t &y);
