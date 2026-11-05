#pragma once
#include <TFT_eSPI.h>

// Shared drawing helpers used by every screen. The screen is 480 x 320 in landscape.
extern TFT_eSPI display;

// A rectangle on screen. The same Button is used to draw it and to test taps, so
// the two can never disagree.
struct Button {
  int16_t left;
  int16_t top;
  int16_t width;
  int16_t height;

  bool contains(uint16_t x, uint16_t y) const {
    return x >= left && x < left + width && y >= top && y < top + height;
  }
};

// Draws the back button; tapping anywhere along the bottom of the screen counts
constexpr Button backButton = {18, 264, 444, 42};
constexpr Button backButtonTapArea = {0, 252, 480, 68};

void beginDisplay();
// Clears the screen and draws the top row (name, Wi-Fi, SD card and battery icons). A title
// goes on a second row with the line under it at y 78; without one (the menu) the line is at y 46.
void drawHeader(const char *title);
// Called from loop(): redraws the status icons when something changes, while the header is showing
void updateStatusIcons();
void markHeaderHidden();  // called on every screen change; drawHeader() shows it again
void drawButton(const Button &button, const char *label, uint16_t fill, uint16_t border,
                uint16_t textColor, bool centred = true);
void drawBackButton();

// Smooth fonts replace the built-in ones while loaded, so this unloads before returning
void drawSmoothCentre(const uint8_t *font, const String &text, int16_t y, uint16_t color);
