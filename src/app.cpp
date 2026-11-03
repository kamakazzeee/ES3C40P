#include "app.h"

#include "screens/menu_screen.h"

namespace {
Screen *activeScreen = &menuScreen;
}  // namespace

void showScreen(Screen &screen) {
  activeScreen->leave();
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
