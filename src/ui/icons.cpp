#include "ui/icons.h"

// Overlapping white shapes use the plain (aliased) fills: a smooth edge blends towards the
// background colour, which would leave dark seams where two white shapes meet.

void drawWeatherIcon(TFT_eSPI &canvas, int16_t x, int16_t y, uint16_t background) {
  int16_t sunX = x - 7;
  int16_t sunY = y - 6;
  for (int ray = 0; ray < 8; ++ray) {
    float angle = ray * PI / 4;
    canvas.drawWideLine(sunX + 12 * cos(angle), sunY + 12 * sin(angle), sunX + 16 * cos(angle),
                        sunY + 16 * sin(angle), 2, TFT_YELLOW, background);
  }
  canvas.fillSmoothCircle(sunX, sunY, 9, TFT_YELLOW, background);
  canvas.fillCircle(x - 2, y + 6, 8, TFT_WHITE);
  canvas.fillCircle(x + 8, y + 1, 11, TFT_WHITE);
  canvas.fillCircle(x + 17, y + 8, 7, TFT_WHITE);
  canvas.fillRoundRect(x - 10, y + 6, 34, 10, 5, TFT_WHITE);
}

void drawPhotoIcon(TFT_eSPI &canvas, int16_t x, int16_t y, uint16_t background) {
  canvas.fillSmoothRoundRect(x - 20, y - 15, 40, 30, 4, TFT_WHITE, background);
  canvas.fillRect(x - 17, y - 12, 34, 24, background);
  canvas.fillTriangle(x - 15, y + 10, x - 5, y - 3, x + 5, y + 10, TFT_WHITE);
  canvas.fillTriangle(x - 2, y + 10, x + 6, y + 1, x + 15, y + 10, TFT_LIGHTGREY);
  canvas.fillCircle(x + 9, y - 6, 3, TFT_YELLOW);
}

void drawSignalIcon(TFT_eSPI &canvas, int16_t x, int16_t y, uint16_t background) {
  for (int bar = 0; bar < 4; ++bar) {
    int16_t height = 9 + bar * 8;
    canvas.fillRect(x - 20 + bar * 11, y + 16 - height, 7, height, TFT_WHITE);
  }
}

void drawThermometerIcon(TFT_eSPI &canvas, int16_t x, int16_t y, uint16_t background) {
  canvas.fillSmoothCircle(x, y + 11, 8, TFT_WHITE, background);
  canvas.fillSmoothCircle(x, y - 13, 5, TFT_WHITE, background);
  canvas.fillRect(x - 5, y - 13, 11, 22, TFT_WHITE);
  canvas.fillRect(x - 1, y - 10, 3, 20, TFT_RED);
  canvas.fillCircle(x, y + 11, 5, TFT_RED);
  for (int tick = 0; tick < 4; ++tick) {
    canvas.drawFastHLine(x + 9, y - 12 + tick * 6, 5, TFT_WHITE);
  }
}

void drawMicrophoneIcon(TFT_eSPI &canvas, int16_t x, int16_t y, uint16_t background) {
  canvas.fillRoundRect(x - 7, y - 19, 14, 24, 7, TFT_WHITE);
  canvas.drawArc(x, y - 6, 13, 10, 270, 90, TFT_WHITE, background);  // the U-shaped holder
  canvas.fillRect(x - 1, y + 7, 3, 7, TFT_WHITE);
  canvas.fillRect(x - 8, y + 14, 17, 3, TFT_WHITE);
}

void drawWifiIcon(TFT_eSPI &canvas, int16_t x, int16_t y, uint16_t background) {
  canvas.drawArc(x, y + 14, 30, 25, 135, 225, TFT_WHITE, background);
  canvas.drawArc(x, y + 14, 20, 15, 135, 225, TFT_WHITE, background);
  canvas.fillSmoothCircle(x, y + 11, 4, TFT_WHITE, background);
}
