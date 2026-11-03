#pragma once
#include <Arduino.h>

// FT6336U capacitive touch controller
void beginTouch();

// True once when a finger first lands, with x and y in screen pixels (480 x 320)
bool readTap(uint16_t &x, uint16_t &y);
