#include "screens/wifi_screen.h"

#include <WiFi.h>

#include "app.h"
#include "net/wifi_connection.h"
#include "screens/keyboard_screen.h"
#include "ui/ui.h"

WifiScreen wifiScreen;

namespace {
constexpr uint8_t networksPerPage = 4;
constexpr unsigned long joinTimeoutMs = 12000;
constexpr uint8_t maxSsidLength = 32;
constexpr uint8_t maxPasswordLength = 63;
constexpr Button previousButton = {18, 218, 102, 32};
constexpr Button rescanButton = {132, 218, 102, 32};
constexpr Button otherButton = {246, 218, 102, 32};
constexpr Button nextButton = {360, 218, 102, 32};
constexpr Button panelConnectButton = {18, 176, 136, 44};
constexpr Button panelForgetButton = {172, 176, 136, 44};
constexpr Button panelCancelButton = {326, 176, 136, 44};

// Used for both drawing a network row and testing taps on it
Button rowButton(uint8_t row) {
  return {18, static_cast<int16_t>(86 + row * 30), 444, 26};
}

void drawPageButton(const Button &button, const char *label, bool enabled) {
  drawButton(button, label, enabled ? TFT_DARKCYAN : TFT_DARKGREY,
             enabled ? TFT_CYAN : TFT_LIGHTGREY, enabled ? TFT_WHITE : TFT_LIGHTGREY);
}

void drawLock(int16_t x, int16_t y, uint16_t color, uint16_t background) {
  display.fillRoundRect(x + 2, y, 8, 9, 3, color);
  display.fillRect(x + 4, y + 2, 4, 5, background);
  display.fillRect(x, y + 6, 12, 8, color);
}

String shortSsid(String ssid) {
  if (ssid.length() > 22) ssid = ssid.substring(0, 19) + "...";
  return ssid;
}

bool isCurrentNetwork(const String &ssid) {
  return wifiConnected() && WiFi.SSID() == ssid;
}
}  // namespace

void WifiScreen::enter() {
  if (keepResults) {
    keepResults = false;
    return;
  }
  notice = autoConnectFailed() ? "No known network in range" : "";
  drawHeader("WI-FI NETWORKS");
  scanNetworks();
}

// The keyboard is a separate screen; coming back keeps the list it was opened from
void WifiScreen::returnFromKeyboard() {
  keepResults = true;
  showScreen(*this);
}

// Hidden networks are left out (OTHER... joins those); one entry per name, strongest first
void WifiScreen::scanNetworks() {
  display.fillRect(0, 82, 480, 182, TFT_BLACK);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawCentreString("Scanning networks...", 240, 145, 2);
  prepareWifiScan();
  int16_t found = WiFi.scanNetworks();
  scanFailed = found < 0;
  networkCount = 0;
  page = 0;
  selected = -1;
  for (int16_t index = 0; index < found; ++index) {
    String ssid = WiFi.SSID(index);
    if (ssid.isEmpty()) continue;
    uint8_t existing = 0;
    while (existing < networkCount && networks[existing].ssid != ssid) ++existing;
    if (existing < networkCount) {
      networks[existing].rssi = max(networks[existing].rssi, WiFi.RSSI(index));
    } else if (networkCount < maxNetworks) {
      networks[networkCount++] = {ssid, WiFi.RSSI(index), WiFi.encryptionType(index) != WIFI_AUTH_OPEN};
    }
  }
  WiFi.scanDelete();
  for (uint8_t i = 1; i < networkCount; ++i) {
    for (uint8_t j = i; j > 0 && networks[j].rssi > networks[j - 1].rssi; --j) {
      std::swap(networks[j], networks[j - 1]);
    }
  }
  Serial.printf("Wi-Fi screen: %d results, %u networks\n", found, networkCount);
}

uint8_t WifiScreen::pageCount() const {
  return networkCount > 0 ? (networkCount + networksPerPage - 1) / networksPerPage : 1;
}

void WifiScreen::draw() {
  drawHeader("WI-FI NETWORKS");
  display.setTextColor(TFT_ORANGE, TFT_BLACK);
  display.drawRightString(notice, 462, 52, 2);

  if (selected >= 0) {
    drawSavedNetworkPanel();
  } else if (networkCount == 0) {
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.drawCentreString(scanFailed ? "SCAN FAILED" : "NO NETWORKS FOUND", 240, 142, 2);
  } else {
    uint8_t first = page * networksPerPage;
    uint8_t last = min<uint8_t>(first + networksPerPage, networkCount);
    for (uint8_t i = first; i < last; ++i) {
      const Network &network = networks[i];
      Button row = rowButton(i - first);
      bool current = isCurrentNetwork(network.ssid);
      String password;
      bool known = knownNetworkPassword(network.ssid, password);
      drawButton(row, shortSsid(network.ssid).c_str(), TFT_NAVY, current ? TFT_GREEN : TFT_BLUE,
                 TFT_WHITE, false);

      int16_t textY = row.top + (row.height - display.fontHeight(2)) / 2;
      int16_t right = row.left + row.width - 12;
      display.setTextColor(network.rssi >= -67 ? TFT_GREEN : network.rssi >= -80 ? TFT_YELLOW : TFT_ORANGE,
                           TFT_NAVY);
      display.drawRightString(String(network.rssi) + " dBm", right, textY, 2);
      if (current) {
        display.setTextColor(TFT_GREEN, TFT_NAVY);
        display.drawRightString("CONNECTED", right - 72, textY, 2);
      } else if (known) {
        display.setTextColor(TFT_CYAN, TFT_NAVY);
        display.drawRightString("SAVED", right - 72, textY, 2);
      } else if (network.secured) {
        drawLock(right - 84, row.top + 6, TFT_LIGHTGREY, TFT_NAVY);
      } else {
        display.setTextColor(TFT_LIGHTGREY, TFT_NAVY);
        display.drawRightString("OPEN", right - 72, textY, 2);
      }
    }
  }

  if (selected < 0) {
    drawPageButton(previousButton, "PREV", page > 0);
    drawPageButton(rescanButton, "RESCAN", true);
    drawPageButton(otherButton, "OTHER...", true);
    drawPageButton(nextButton, "NEXT", page + 1 < pageCount());
  }
  drawBackButton();
}

void WifiScreen::drawSavedNetworkPanel() {
  const Network &network = networks[selected];
  bool current = isCurrentNetwork(network.ssid);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawCentreString(shortSsid(network.ssid), 240, 100, 4);
  display.setTextColor(current ? TFT_GREEN : TFT_CYAN, TFT_BLACK);
  display.drawCentreString(current ? "Connected, saved network" : "Saved network", 240, 136, 2);
  drawPageButton(panelConnectButton, "CONNECT", !current);
  drawButton(panelForgetButton, "FORGET", TFT_MAROON, TFT_RED, TFT_WHITE);
  drawPageButton(panelCancelButton, "CANCEL", true);
}

void WifiScreen::onSavedNetworkPanelTap(uint16_t x, uint16_t y) {
  const Network network = networks[selected];
  if (panelConnectButton.contains(x, y) && !isCurrentNetwork(network.ssid)) {
    selected = -1;
    String password;
    knownNetworkPassword(network.ssid, password);
    join(network.ssid, password);
  } else if (panelForgetButton.contains(x, y)) {
    forgetWifiNetwork(network.ssid);
    selected = -1;
    notice = "Forgot " + shortSsid(network.ssid);
    draw();
  } else if (panelCancelButton.contains(x, y)) {
    selected = -1;
    draw();
  } else if (backButtonTapArea.contains(x, y)) {
    selected = -1;
    if (!wifiConnected()) retryAutoConnect();
    showMenu();
  }
}

void WifiScreen::onTap(uint16_t x, uint16_t y) {
  if (selected >= 0) {
    onSavedNetworkPanelTap(x, y);
    return;
  }
  uint8_t first = page * networksPerPage;
  uint8_t last = min<uint8_t>(first + networksPerPage, networkCount);
  for (uint8_t i = first; i < last; ++i) {
    if (rowButton(i - first).contains(x, y)) {
      choose(i);
      return;
    }
  }
  if (previousButton.contains(x, y) && page > 0) {
    --page;
    draw();
  } else if (rescanButton.contains(x, y)) {
    notice = "";
    scanNetworks();
    draw();
  } else if (otherButton.contains(x, y)) {
    askHiddenNetwork();
  } else if (nextButton.contains(x, y) && page + 1 < pageCount()) {
    ++page;
    draw();
  } else if (backButtonTapArea.contains(x, y)) {
    if (!wifiConnected()) retryAutoConnect();
    showMenu();
  }
}

// Saved networks open a CONNECT / FORGET panel; everything else connects straight away
void WifiScreen::choose(uint8_t index) {
  const Network &network = networks[index];
  if (canForgetNetwork(network.ssid)) {
    selected = index;
    draw();
    return;
  }
  if (isCurrentNetwork(network.ssid)) {
    notice = "Already connected";
    draw();
    return;
  }
  String password;
  if (!network.secured || knownNetworkPassword(network.ssid, password)) {
    join(network.ssid, password);
  } else {
    askPassword(network.ssid, "Password for " + shortSsid(network.ssid));
  }
}

// A locked network that will not connect most likely has the wrong password, so ask again
void WifiScreen::join(const String &ssid, const String &password) {
  display.fillRect(0, 82, 480, 182, TFT_BLACK);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawCentreString("Connecting to " + shortSsid(ssid) + "...", 240, 145, 2);
  if (joinNetwork(ssid, password, joinTimeoutMs)) {
    showMenu();
  } else if (!password.isEmpty()) {
    askPassword(ssid, "Could not join " + shortSsid(ssid) + ". Check the password:");
  } else {
    notice = "Could not join " + shortSsid(ssid);
    draw();
  }
}

void WifiScreen::askPassword(const String &ssid, const String &title) {
  keyboardScreen.open(title, "", true, maxPasswordLength, [this, ssid](bool accepted, const String &text) {
    returnFromKeyboard();
    if (accepted) join(ssid, text);
  });
}

// For a network that does not broadcast its name; leave the password blank if it is open
void WifiScreen::askHiddenNetwork() {
  keyboardScreen.open("Network name (SSID)", "", false, maxSsidLength, [this](bool accepted, const String &ssid) {
    if (!accepted || ssid.isEmpty()) {
      returnFromKeyboard();
      return;
    }
    keyboardScreen.open("Password for " + shortSsid(ssid) + " (blank if open)", "", true, maxPasswordLength,
                        [this, ssid](bool accepted, const String &password) {
                          returnFromKeyboard();
                          if (accepted) join(ssid, password);
                        });
  });
}

void WifiScreen::leave() {
  WiFi.scanDelete();
}
