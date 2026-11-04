#pragma once
#include <functional>

#include "screens/screen.h"

// On-screen QWERTY keyboard for typing a line of text. open() shows it; OK or CANCEL
// calls onDone, which must then show another screen.
class KeyboardScreen : public Screen {
 public:
  typedef std::function<void(bool accepted, const String &text)> DoneCallback;

  void open(const String &title, const String &initialText, bool masked, uint8_t maxLength,
            DoneCallback onDone);
  void draw() override;
  void onTap(uint16_t x, uint16_t y) override;

  enum class Page : uint8_t { lower, upper, symbols, moreSymbols };

 private:
  void drawField();
  void drawKeys();
  void finish(bool accepted);
  String title;
  String text;
  bool masked = false;
  bool revealed = false;
  uint8_t maxLength = 0;
  Page page = Page::lower;
  DoneCallback onDone;
};

extern KeyboardScreen keyboardScreen;
