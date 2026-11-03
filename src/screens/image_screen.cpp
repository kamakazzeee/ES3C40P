#include "screens/image_screen.h"

#include <SD_MMC.h>
#include <TJpg_Decoder.h>

#include "app.h"
#include "hardware/sd_storage.h"
#include "ui/ui.h"

ImageScreen imageScreen;

namespace {
constexpr unsigned long imageIntervalMs = 5000;

// The JPEG decoder hands over the picture in small blocks; this copies each one to the screen
bool tftOutput(int16_t x, int16_t y, uint16_t width, uint16_t height, uint16_t *bitmap) {
  if (y >= display.height()) return false;
  display.pushImage(x, y, width, height, bitmap);
  return true;
}

void showImageError(const char *message) {
  display.setTextColor(TFT_ORANGE, TFT_BLACK);
  display.drawCentreString(message, 240, 150, 2);
}
}  // namespace

void ImageScreen::enter() {
  TJpgDec.setCallback(tftOutput);
  scanImages();
  imageIndex = 0;
  imageShownAt = millis();
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

void ImageScreen::draw() {
  drawHeader("SD IMAGE VIEWER");
  if (!sdCardReady()) {
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.drawCentreString("SD CARD NOT DETECTED", 240, 148, 4);
  } else if (imageCount == 0) {
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.drawCentreString("NO JPEG IMAGES FOUND", 240, 148, 4);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawCentreString("Place .jpg files in the card root", 240, 186, 2);
  } else {
    drawImage(imagePaths[imageIndex]);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawRightString(String(imageIndex + 1) + " / " + String(imageCount), 460, 84, 2);
  }
  drawBackButton();
}

// Shrinks the picture by 2, 4 or 8 until it fits the 480 x 176 area, then centres it
void ImageScreen::drawImage(const String &path) {
  display.fillRect(0, 82, 480, 184, TFT_BLACK);
  uint16_t imageWidth = 0;
  uint16_t imageHeight = 0;
  File sizeFile = SD_MMC.open(path, FILE_READ);
  if (!sizeFile) {
    Serial.printf("Could not open JPEG: %s\n", path.c_str());
    showImageError("COULD NOT OPEN JPEG");
    return;
  }
  JRESULT sizeResult = TJpgDec.getSdJpgSize(&imageWidth, &imageHeight, sizeFile);
  sizeFile.close();
  if (sizeResult != JDR_OK) {
    Serial.printf("JPEG header failed for %s: %d\n", path.c_str(), sizeResult);
    showImageError("JPEG FORMAT UNSUPPORTED");
    return;
  }

  uint8_t scale = 1;
  while ((imageWidth / scale > 480 || imageHeight / scale > 176) && scale < 8) scale *= 2;
  uint16_t scaledWidth = imageWidth / scale;
  uint16_t scaledHeight = imageHeight / scale;
  int16_t imageX = (480 - scaledWidth) / 2;
  int16_t imageY = 82 + (176 - scaledHeight) / 2;
  TJpgDec.setJpgScale(scale);
  display.setSwapBytes(true);

  File imageFile = SD_MMC.open(path, FILE_READ);
  JRESULT result = imageFile ? TJpgDec.drawSdJpg(imageX, imageY, imageFile) : JDR_INP;
  if (imageFile) imageFile.close();
  if (result != JDR_OK) {
    Serial.printf("JPEG decode failed for %s: %d\n", path.c_str(), result);
    showImageError("JPEG DECODE FAILED");
  }
}

void ImageScreen::onTap(uint16_t x, uint16_t y) {
  if (backButtonTapArea.contains(x, y)) showMenu();
}

void ImageScreen::update() {
  if (imageCount == 0 || millis() - imageShownAt < imageIntervalMs) return;
  ++imageIndex;
  if (imageIndex >= imageCount) {
    showMenu();
  } else {
    imageShownAt = millis();
    draw();
  }
}
