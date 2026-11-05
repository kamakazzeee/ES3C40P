#pragma once
#include <TFT_eSPI.h>

// Small icons drawn from shapes, about 44 x 36 pixels around (x, y). They draw onto the screen
// or onto a sprite; smooth edges blend into the background colour, which must match what's behind.
typedef void (*IconDrawer)(TFT_eSPI &canvas, int16_t x, int16_t y, uint16_t background);

void drawWeatherIcon(TFT_eSPI &canvas, int16_t x, int16_t y, uint16_t background);
void drawPhotoIcon(TFT_eSPI &canvas, int16_t x, int16_t y, uint16_t background);
void drawSignalIcon(TFT_eSPI &canvas, int16_t x, int16_t y, uint16_t background);
void drawThermometerIcon(TFT_eSPI &canvas, int16_t x, int16_t y, uint16_t background);
void drawMicrophoneIcon(TFT_eSPI &canvas, int16_t x, int16_t y, uint16_t background);
void drawWifiIcon(TFT_eSPI &canvas, int16_t x, int16_t y, uint16_t background);
void drawBatteryTestIcon(TFT_eSPI &canvas, int16_t x, int16_t y, uint16_t background);
void drawGraphIcon(TFT_eSPI &canvas, int16_t x, int16_t y, uint16_t background);
