#pragma once
#include "screens/screen.h"

// Choose a Wi-Fi network: opened by tapping the Wi-Fi status on the home menu, or by itself
// when no known network can be joined at boot. Locked networks ask for a password on the
// on-screen keyboard; anything joined here is saved, and tapping a saved network offers FORGET.
class WifiScreen : public Screen {
 public:
  void enter() override;
  void draw() override;
  void onTap(uint16_t x, uint16_t y) override;
  void leave() override;

 private:
  struct Network {
    String ssid;
    int32_t rssi;
    bool secured;
  };
  static constexpr uint8_t maxNetworks = 24;

  void returnFromKeyboard();
  void scanNetworks();
  void choose(uint8_t index);
  void drawSavedNetworkPanel();
  void onSavedNetworkPanelTap(uint16_t x, uint16_t y);
  void join(const String &ssid, const String &password);
  void askPassword(const String &ssid, const String &title);
  void askHiddenNetwork();
  uint8_t pageCount() const;
  Network networks[maxNetworks];
  uint8_t networkCount = 0;
  bool scanFailed = false;
  bool keepResults = false;  // coming back from the keyboard, so skip the rescan
  uint8_t page = 0;
  int8_t selected = -1;  // saved network whose CONNECT / FORGET panel is open
  String notice;
};

extern WifiScreen wifiScreen;
