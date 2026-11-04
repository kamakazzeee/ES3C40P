#pragma once
#include "screens/screen.h"

// Slideshow of the .jpg files in the SD card root, one every 5 seconds, then back to the menu.
// Taps work even while a picture is decoding: BACK leaves, anywhere else skips ahead.
// Big pictures are resized to fit the first time and saved to /.thumbs (see ui/picture.h).
class ImageScreen : public Screen {
 public:
  void enter() override;
  void draw() override;
  void onTap(uint16_t x, uint16_t y) override;
  void update() override;

 private:
  static constexpr uint8_t maximumImages = 24;
  void scanImages();
  void drawImage(const String &path);
  void showNextImage();
  String imagePaths[maximumImages];
  uint8_t imageCount = 0;
  uint8_t imageIndex = 0;
  unsigned long imageShownAt = 0;
};

extern ImageScreen imageScreen;
