#pragma once
#include "screens/screen.h"

// Keeps track of which screen is showing and switches between them
void showScreen(Screen &screen);  // leave the old screen, then enter and draw the new one
void showMenu();
Screen &currentScreen();
