#pragma once
#include "screens/screen.h"

class ScannerScreen : public Screen {
 public:
  void enter() override;
  void draw() override;
  void onTap(uint16_t x, uint16_t y) override;
  void leave() override;

 private:
  void scanNetworks();
  uint8_t pageCount() const;
  int networkCount = -1;  // -1 means the scan failed
  uint8_t page = 0;
};

extern ScannerScreen scannerScreen;
