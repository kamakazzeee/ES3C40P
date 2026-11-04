#include "screens/keyboard_screen.h"

#include "app.h"
#include "ui/ui.h"

KeyboardScreen keyboardScreen;

namespace {
typedef KeyboardScreen::Page Page;

enum class KeyAction : uint8_t { character, shift, backspace, mode, space, cancel, done };

struct Key {
  Button button;
  KeyAction action;
  char character;
};

constexpr int16_t keyStep = 47;
constexpr int16_t keyWidth = 43;
constexpr int16_t keyHeight = 44;
constexpr int16_t keyboardLeft = 7;
constexpr int16_t keyboardWidth = 10 * keyStep - 4;
constexpr int16_t rowTops[] = {112, 162, 212, 262};
constexpr uint8_t maxKeys = 40;

constexpr Button textField = {8, 34, 380, 44};
constexpr Button revealButton = {396, 34, 76, 44};

// Three rows of characters per page; together the two symbol pages hold all of printable ASCII
const char *const pageRows[][3] = {
    {"qwertyuiop", "asdfghjkl", "zxcvbnm"},
    {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"},
    {"1234567890", "-/:;()$&@", "._,?!'\""},
    {"[]{}#%^*+=", "\\|~<>`", "._,?!'\""},
};

uint8_t addCharacterRow(Key *keys, uint8_t count, const char *characters, int16_t top) {
  int16_t length = strlen(characters);
  int16_t left = 240 - (length * keyStep - 4) / 2;
  for (int16_t i = 0; i < length; ++i) {
    keys[count++] = {{static_cast<int16_t>(left + i * keyStep), top, keyWidth, keyHeight},
                     KeyAction::character, characters[i]};
  }
  return count;
}

// The same layout is used to draw the keys and to find the key under a tap
uint8_t layoutKeys(Page page, Key *keys) {
  const char *const *rows = pageRows[static_cast<uint8_t>(page)];
  uint8_t count = 0;
  count = addCharacterRow(keys, count, rows[0], rowTops[0]);
  count = addCharacterRow(keys, count, rows[1], rowTops[1]);
  constexpr int16_t sideWidth = 66;
  keys[count++] = {{keyboardLeft, rowTops[2], sideWidth, keyHeight}, KeyAction::shift, 0};
  count = addCharacterRow(keys, count, rows[2], rowTops[2]);
  keys[count++] = {{keyboardLeft + keyboardWidth - sideWidth, rowTops[2], sideWidth, keyHeight},
                   KeyAction::backspace, 0};
  keys[count++] = {{keyboardLeft, rowTops[3], 90, keyHeight}, KeyAction::mode, 0};
  keys[count++] = {{101, rowTops[3], 182, keyHeight}, KeyAction::space, ' '};
  keys[count++] = {{287, rowTops[3], 90, keyHeight}, KeyAction::cancel, 0};
  keys[count++] = {{381, rowTops[3], 92, keyHeight}, KeyAction::done, 0};
  return count;
}

bool lettersPage(Page page) {
  return page == Page::lower || page == Page::upper;
}

String keyLabel(const Key &key, Page page) {
  switch (key.action) {
    case KeyAction::character: return String(key.character);
    case KeyAction::shift: return lettersPage(page) ? "SHIFT" : page == Page::symbols ? "#+=" : "123";
    case KeyAction::backspace: return "DEL";
    case KeyAction::mode: return lettersPage(page) ? "?123" : "ABC";
    case KeyAction::space: return "SPACE";
    case KeyAction::cancel: return "CANCEL";
    case KeyAction::done: return "OK";
  }
  return "";
}

void drawKey(const Key &key, Page page, bool pressed) {
  uint16_t fill = TFT_DARKGREY;
  uint16_t border = TFT_LIGHTGREY;
  if (key.action == KeyAction::done) {
    fill = TFT_DARKGREEN;
    border = TFT_GREEN;
  } else if (key.action == KeyAction::shift && page == Page::upper) {
    fill = TFT_DARKCYAN;
    border = TFT_CYAN;
  } else if (key.action != KeyAction::character && key.action != KeyAction::space) {
    fill = TFT_NAVY;
    border = TFT_BLUE;
  }
  if (pressed) fill = TFT_LIGHTGREY;
  drawButton(key.button, keyLabel(key, page).c_str(), fill, border, pressed ? TFT_BLACK : TFT_WHITE);
}
}  // namespace

void KeyboardScreen::open(const String &newTitle, const String &initialText, bool newMasked,
                          uint8_t newMaxLength, DoneCallback newOnDone) {
  title = newTitle;
  text = initialText;
  masked = newMasked;
  revealed = false;
  maxLength = newMaxLength;
  page = Page::lower;
  onDone = newOnDone;
  showScreen(*this);
}

void KeyboardScreen::draw() {
  display.fillScreen(TFT_BLACK);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.drawString(title, 8, 10, 2);
  if (masked) {
    drawButton(revealButton, revealed ? "HIDE" : "SHOW", TFT_NAVY, TFT_BLUE, TFT_WHITE);
  }
  drawField();
  drawKeys();
}

// Shows the end of the text when it is wider than the field
void KeyboardScreen::drawField() {
  String shown = text;
  if (masked && !revealed) {
    shown = "";
    for (size_t i = 0; i < text.length(); ++i) shown += '*';
  }
  int16_t room = textField.width - 24;
  while (shown.length() > 0 && display.textWidth(shown, 4) > room) shown.remove(0, 1);

  display.fillRect(textField.left, textField.top, textField.width, textField.height, TFT_BLACK);
  display.drawRect(textField.left, textField.top, textField.width, textField.height, TFT_LIGHTGREY);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  int16_t textY = textField.top + (textField.height - display.fontHeight(4)) / 2;
  display.drawString(shown, textField.left + 10, textY, 4);
  int16_t cursorX = textField.left + 12 + display.textWidth(shown, 4);
  display.fillRect(cursorX, textY, 2, display.fontHeight(4), TFT_CYAN);
}

void KeyboardScreen::drawKeys() {
  display.fillRect(0, rowTops[0] - 4, 480, 320 - rowTops[0] + 4, TFT_BLACK);
  Key keys[maxKeys];
  uint8_t count = layoutKeys(page, keys);
  for (uint8_t i = 0; i < count; ++i) drawKey(keys[i], page, false);
}

void KeyboardScreen::onTap(uint16_t x, uint16_t y) {
  if (masked && revealButton.contains(x, y)) {
    revealed = !revealed;
    drawButton(revealButton, revealed ? "HIDE" : "SHOW", TFT_NAVY, TFT_BLUE, TFT_WHITE);
    drawField();
    return;
  }

  Key keys[maxKeys];
  uint8_t count = layoutKeys(page, keys);
  for (uint8_t i = 0; i < count; ++i) {
    const Key &key = keys[i];
    if (!key.button.contains(x, y)) continue;
    drawKey(key, page, true);
    delay(60);
    drawKey(key, page, false);

    switch (key.action) {
      case KeyAction::character:
      case KeyAction::space:
        if (text.length() < maxLength) text += key.character;
        drawField();
        // Shift applies to one letter, as on a phone
        if (page == Page::upper) {
          page = Page::lower;
          drawKeys();
        }
        break;
      case KeyAction::backspace:
        if (text.length() > 0) text.remove(text.length() - 1);
        drawField();
        break;
      case KeyAction::shift:
        page = page == Page::lower ? Page::upper
             : page == Page::upper ? Page::lower
             : page == Page::symbols ? Page::moreSymbols
                                     : Page::symbols;
        drawKeys();
        break;
      case KeyAction::mode:
        page = lettersPage(page) ? Page::symbols : Page::lower;
        drawKeys();
        break;
      case KeyAction::cancel: finish(false); break;
      case KeyAction::done: finish(true); break;
    }
    return;
  }
}

// onDone shows the next screen, which may reopen this keyboard and replace onDone,
// so call a copy
void KeyboardScreen::finish(bool accepted) {
  DoneCallback callback = onDone;
  String result = text;
  if (callback) {
    callback(accepted, result);
  } else {
    showMenu();
  }
}
