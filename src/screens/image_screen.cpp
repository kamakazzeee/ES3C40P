#include "screens/image_screen.h"

#include <SD_MMC.h>

#include "app.h"
#include "hardware/sd_storage.h"
#include "hardware/touch.h"
#include "ui/picture.h"
#include "ui/ui.h"

ImageScreen imageScreen;

namespace {
constexpr unsigned long imageIntervalMs = 5000;
constexpr Button imageArea = {0, 82, 480, 176};  // between the header line and the back button

// A tap made while a picture is decoding; update() hands it to onTap() once the decode stops
bool tapDuringDecode = false;
uint16_t decodeTapX = 0;
uint16_t decodeTapY = 0;

// Checked between decoded blocks, so a slow picture can be left or skipped
// (readTap() only touches I2C after the touch interrupt has fired)
bool tapStopsDecode() {
  if (!readTap(decodeTapX, decodeTapY)) return false;
  tapDuringDecode = true;
  return true;
}

void showImageError(const char *message) {
  display.setTextColor(TFT_ORANGE, TFT_BLACK);
  display.drawCentreString(message, 240, 150, 2);
}
}  // namespace

void ImageScreen::enter() {
  scanImages();
  imageIndex = 0;
  tapDuringDecode = false;
}

void ImageScreen::scanImages() {
  imageCount = 0;
  if (!sdCardReady()) return;
  File directory = SD_MMC.open("/");
  if (!directory) return;
  while (imageCount < maximumImages) {
    File file = directory.openNextFile();
    if (!file) break;
    String filename = file.name();
    bool isImage = !file.isDirectory() && (filename.endsWith(".jpg") || filename.endsWith(".JPG") ||
                                           filename.endsWith(".jpeg") || filename.endsWith(".JPEG"));
    if (isImage) {
      if (!filename.startsWith("/")) filename = "/" + filename;
      Serial.printf("Found JPEG: %s\n", filename.c_str());
      imagePaths[imageCount++] = filename;
    }
    file.close();
  }
  directory.close();
}

// The header, counter and back button go up first so they are there while a slow picture decodes
void ImageScreen::draw() {
  drawHeader("SD IMAGE VIEWER");
  drawBackButton();
  if (!sdCardReady()) {
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.drawCentreString("SD CARD NOT DETECTED", 240, 148, 4);
  } else if (imageCount == 0) {
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.drawCentreString("NO JPEG IMAGES FOUND", 240, 148, 4);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawCentreString("Place .jpg files in the card root", 240, 186, 2);
  } else {
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawRightString(String(imageIndex + 1) + " / " + String(imageCount), 460, 52, 2);
    drawImage(imagePaths[imageIndex]);
  }
  imageShownAt = millis();  // after decoding, so a slow picture still gets its full 5 s
}

void ImageScreen::drawImage(const String &path) {
  switch (drawPicture(path, imageArea, tapStopsDecode)) {
    case PictureResult::openFailed: showImageError("COULD NOT OPEN JPEG"); break;
    case PictureResult::unsupported: showImageError("JPEG FORMAT UNSUPPORTED"); break;
    case PictureResult::decodeFailed: showImageError("JPEG DECODE FAILED"); break;
    case PictureResult::shown:
    case PictureResult::stopped: break;
  }
}

// BACK returns to the menu; a tap anywhere else skips to the next picture
void ImageScreen::onTap(uint16_t x, uint16_t y) {
  if (backButtonTapArea.contains(x, y)) {
    showMenu();
  } else if (imageCount > 0) {
    showNextImage();
  }
}

void ImageScreen::update() {
  if (tapDuringDecode) {
    tapDuringDecode = false;
    onTap(decodeTapX, decodeTapY);
    return;
  }
  if (imageCount == 0 || millis() - imageShownAt < imageIntervalMs) return;
  showNextImage();
}

// After the last picture the slideshow returns to the menu
void ImageScreen::showNextImage() {
  ++imageIndex;
  if (imageIndex >= imageCount) {
    showMenu();
  } else {
    draw();
  }
}
