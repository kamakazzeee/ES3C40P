#include "app.h"

#include "hardware/touch.h"
#include "screens/menu_screen.h"
#include "ui/ui.h"

namespace {
constexpr int16_t swipeMinimumPx = 60;  // sideways distance that counts as a swipe
constexpr int16_t tapSlopPx = 20;       // a finger can wobble this far and still tap

Screen *activeScreen = &menuScreen;

// Where the finger landed on a screen that uses swipes; cleared on release
Screen *pressedScreen = nullptr;
uint16_t pressX = 0;
uint16_t pressY = 0;
}  // namespace

void showScreen(Screen &screen) {
  activeScreen->leave();
  markHeaderHidden();
  activeScreen = &screen;
  activeScreen->enter();
  activeScreen->draw();
}

void showMenu() {
  showScreen(menuScreen);
}

Screen &currentScreen() {
  return *activeScreen;
}

void handleTouch() {
  uint16_t x;
  uint16_t y;
  TouchEvent event = readTouchEvent(x, y);
  if (event == TouchEvent::press) {
    if (activeScreen->usesSwipes()) {
      pressedScreen = activeScreen;
      pressX = x;
      pressY = y;
    } else {
      activeScreen->onTap(x, y);
    }
  } else if (event == TouchEvent::release) {
    // Ignored if the screen changed while the finger was down (e.g. BACK opened the menu)
    if (pressedScreen == activeScreen) {
      int16_t dx = int16_t(x) - int16_t(pressX);
      int16_t dy = int16_t(y) - int16_t(pressY);
      if (abs(dx) >= swipeMinimumPx && abs(dx) > 2 * abs(dy)) {
        activeScreen->onSwipe(dx < 0 ? -1 : 1);
      } else if (abs(dx) <= tapSlopPx && abs(dy) <= tapSlopPx) {
        activeScreen->onTap(pressX, pressY);
      }
    }
    pressedScreen = nullptr;
  }
}
