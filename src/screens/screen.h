#pragma once
#include <Arduino.h>

// Every screen in the app follows this shape. app.cpp calls these at the right times,
// so a new screen only has to fill in the parts it needs.
class Screen {
 public:
  virtual ~Screen() {}
  virtual void enter() {}  // once, when the screen opens: scan, fetch, load
  virtual void draw() = 0;  // paint the whole screen
  virtual void onTap(uint16_t x, uint16_t y) = 0;
  virtual void update() {}  // every loop(): slideshows, live readouts
  virtual void leave() {}  // tidy up before another screen opens
};
