#include "ui/picture.h"

#include <SD_MMC.h>
#include <TJpg_Decoder.h>

namespace {
constexpr char cacheFolder[] = "/.thumbs";
constexpr uint32_t cacheMagic = 0x31424854;  // "THB1"
constexpr uint16_t cacheRowsPerRead = 4;

// Saved copies start with this; a copy is only used if the source file's size and date match
struct CacheHeader {
  uint32_t magic;
  uint32_t sourceSize;
  uint32_t sourceTime;
  uint16_t width;
  uint16_t height;
};

// Decoded blocks go either to the screen, clipped to clipBottom, or into decodeBuffer
uint16_t *decodeBuffer = nullptr;
int16_t bufferWidth = 0;
int16_t bufferHeight = 0;
int16_t clipBottom = 0;
bool passedClipBottom = false;
bool stoppedByCaller = false;
PictureStopCheck stopCheck;

bool onBlock(int16_t x, int16_t y, uint16_t width, uint16_t height, uint16_t *bitmap) {
  if (stopCheck && stopCheck()) {
    stoppedByCaller = true;
    return false;
  }
  if (decodeBuffer) {
    if (x >= bufferWidth) return true;
    uint16_t columns = min<int32_t>(width, bufferWidth - x);
    for (uint16_t row = 0; row < height && y + row < bufferHeight; ++row) {
      memcpy(decodeBuffer + (y + row) * bufferWidth + x, bitmap + row * width, columns * sizeof(uint16_t));
    }
    return true;
  }
  if (y >= clipBottom) {
    passedClipBottom = true;
    return false;
  }
  display.pushImage(x, y, width, height, bitmap);
  return true;
}

void *allocate(size_t bytes) {
  void *memory = ps_malloc(bytes);
  return memory ? memory : malloc(bytes);
}

String cachePathFor(const String &path) {
  return String(cacheFolder) + (path.startsWith("/") ? "" : "/") + path + ".bin";
}

JRESULT decodeFile(const String &path, int16_t x, int16_t y, uint8_t scale) {
  File file = SD_MMC.open(path, FILE_READ);
  if (!file) return JDR_INP;
  TJpgDec.setJpgScale(scale);
  JRESULT result = TJpgDec.drawSdJpg(x, y, file);
  file.close();
  return result;
}

bool drawCachedCopy(const String &cachePath, const CacheHeader &expected, const Button &box) {
  File file = SD_MMC.open(cachePath, FILE_READ);
  if (!file) return false;
  CacheHeader header;
  bool valid = file.read(reinterpret_cast<uint8_t *>(&header), sizeof(header)) == sizeof(header) &&
               header.magic == cacheMagic && header.sourceSize == expected.sourceSize &&
               header.sourceTime == expected.sourceTime && header.width <= box.width &&
               header.height <= box.height &&
               file.size() == sizeof(header) + uint32_t(header.width) * header.height * sizeof(uint16_t);
  uint16_t *rows = valid ? static_cast<uint16_t *>(allocate(header.width * cacheRowsPerRead * sizeof(uint16_t))) : nullptr;
  if (!rows) {
    file.close();
    return false;
  }
  int16_t left = box.left + (box.width - header.width) / 2;
  int16_t top = box.top + (box.height - header.height) / 2;
  for (uint16_t row = 0; row < header.height; row += cacheRowsPerRead) {
    uint16_t count = min<uint16_t>(cacheRowsPerRead, header.height - row);
    size_t bytes = header.width * count * sizeof(uint16_t);
    if (file.read(reinterpret_cast<uint8_t *>(rows), bytes) != bytes) break;
    display.pushImage(left, top + row, header.width, count, rows);
  }
  free(rows);
  file.close();
  return true;
}

void saveCachedCopy(const String &cachePath, const CacheHeader &header, const uint16_t *pixels) {
  if (!SD_MMC.exists(cacheFolder)) SD_MMC.mkdir(cacheFolder);
  File file = SD_MMC.open(cachePath, FILE_WRITE);
  size_t pixelBytes = uint32_t(header.width) * header.height * sizeof(uint16_t);
  bool saved = file && file.write(reinterpret_cast<const uint8_t *>(&header), sizeof(header)) == sizeof(header) &&
               file.write(reinterpret_cast<const uint8_t *>(pixels), pixelBytes) == pixelBytes;
  if (file) file.close();
  if (!saved) {
    Serial.printf("Could not save %s\n", cachePath.c_str());
    SD_MMC.remove(cachePath);
  }
}

// Box filter: each output pixel is the average of the RGB565 source pixels it covers
void shrink(const uint16_t *source, int16_t sourceStride, int16_t sourceWidth, int16_t sourceHeight,
            uint16_t *output, int16_t outputWidth, int16_t outputHeight) {
  for (int16_t outY = 0; outY < outputHeight; ++outY) {
    int32_t y0 = int32_t(outY) * sourceHeight / outputHeight;
    int32_t y1 = max<int32_t>(y0 + 1, int32_t(outY + 1) * sourceHeight / outputHeight);
    for (int16_t outX = 0; outX < outputWidth; ++outX) {
      int32_t x0 = int32_t(outX) * sourceWidth / outputWidth;
      int32_t x1 = max<int32_t>(x0 + 1, int32_t(outX + 1) * sourceWidth / outputWidth);
      uint32_t red = 0;
      uint32_t green = 0;
      uint32_t blue = 0;
      for (int32_t y = y0; y < y1; ++y) {
        for (int32_t x = x0; x < x1; ++x) {
          uint16_t pixel = source[y * sourceStride + x];
          red += pixel >> 11;
          green += (pixel >> 5) & 0x3F;
          blue += pixel & 0x1F;
        }
      }
      uint32_t count = (y1 - y0) * (x1 - x0);
      output[outY * outputWidth + outX] = ((red / count) << 11) | ((green / count) << 5) | (blue / count);
    }
  }
}

// Decodes at 1/2, 1/4 or 1/8 into PSRAM (staying at or above the target size), shrinks the
// rest of the way, draws it and saves it. Returns false only if memory ran out.
bool drawResized(const String &path, uint16_t imageWidth, uint16_t imageHeight, const Button &box,
                 const CacheHeader &sourceInfo, PictureResult &result) {
  float fit = min(box.width / float(imageWidth), box.height / float(imageHeight));
  int16_t targetWidth = max(1, int(imageWidth * fit));
  int16_t targetHeight = max(1, int(imageHeight * fit));
  uint8_t scale = 1;
  while (scale < 8 && imageWidth / (scale * 2) >= targetWidth && imageHeight / (scale * 2) >= targetHeight) {
    scale *= 2;
  }
  int16_t decodedWidth = imageWidth / scale;
  int16_t decodedHeight = imageHeight / scale;

  // Rounded up so edge blocks always land inside the buffer
  bufferWidth = (imageWidth + scale - 1) / scale;
  bufferHeight = (imageHeight + scale - 1) / scale;
  uint16_t *decoded = static_cast<uint16_t *>(allocate(uint32_t(bufferWidth) * bufferHeight * sizeof(uint16_t)));
  uint16_t *resized = static_cast<uint16_t *>(allocate(uint32_t(targetWidth) * targetHeight * sizeof(uint16_t)));
  if (!decoded || !resized) {
    free(decoded);
    free(resized);
    Serial.printf("Not enough memory to resize %s\n", path.c_str());
    return false;
  }

  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  display.drawCentreString("Resizing picture (first time only)...", box.left + box.width / 2,
                           box.top + box.height / 2 - 8, 2);
  unsigned long startedAt = millis();
  decodeBuffer = decoded;
  JRESULT decodeResult = decodeFile(path, 0, 0, scale);
  decodeBuffer = nullptr;

  if (stoppedByCaller) {
    result = PictureResult::stopped;
  } else if (decodeResult != JDR_OK) {
    Serial.printf("JPEG decode failed for %s: %d\n", path.c_str(), decodeResult);
    result = PictureResult::decodeFailed;
  } else {
    shrink(decoded, bufferWidth, decodedWidth, decodedHeight, resized, targetWidth, targetHeight);
    display.fillRect(box.left, box.top, box.width, box.height, TFT_BLACK);
    display.pushImage(box.left + (box.width - targetWidth) / 2, box.top + (box.height - targetHeight) / 2,
                      targetWidth, targetHeight, resized);
    CacheHeader header = sourceInfo;
    header.width = targetWidth;
    header.height = targetHeight;
    saveCachedCopy(cachePathFor(path), header, resized);
    Serial.printf("Resized %s from %ux%u to %dx%d in %lu ms\n", path.c_str(), imageWidth, imageHeight,
                  targetWidth, targetHeight, millis() - startedAt);
    result = PictureResult::shown;
  }
  free(decoded);
  free(resized);
  return true;
}

// For pictures that already fit, or as a fallback: decodes straight to the screen. One still
// too big at 1/8 is cropped to its centre, and the viewport keeps it inside the box.
PictureResult drawDirect(const String &path, uint16_t imageWidth, uint16_t imageHeight, const Button &box) {
  uint8_t scale = 1;
  while ((imageWidth / scale > box.width || imageHeight / scale > box.height) && scale < 8) scale *= 2;
  int16_t x = box.left + (box.width - int16_t(imageWidth / scale)) / 2;
  int16_t y = box.top + (box.height - int16_t(imageHeight / scale)) / 2;
  clipBottom = box.top + box.height;
  passedClipBottom = false;
  display.setViewport(box.left, box.top, box.width, box.height, false);
  JRESULT result = decodeFile(path, x, y, scale);
  display.resetViewport();
  if (stoppedByCaller) return PictureResult::stopped;
  if (result != JDR_OK && !passedClipBottom) {
    Serial.printf("JPEG decode failed for %s: %d\n", path.c_str(), result);
    return PictureResult::decodeFailed;
  }
  return PictureResult::shown;
}
}  // namespace

PictureResult drawPicture(const String &path, const Button &box, PictureStopCheck shouldStop) {
  display.fillRect(box.left, box.top, box.width, box.height, TFT_BLACK);
  File source = SD_MMC.open(path, FILE_READ);
  if (!source) {
    Serial.printf("Could not open JPEG: %s\n", path.c_str());
    return PictureResult::openFailed;
  }
  CacheHeader sourceInfo = {cacheMagic, uint32_t(source.size()), uint32_t(source.getLastWrite()), 0, 0};
  uint16_t imageWidth = 0;
  uint16_t imageHeight = 0;
  JRESULT sizeResult = TJpgDec.getSdJpgSize(&imageWidth, &imageHeight, source);
  source.close();
  if (sizeResult != JDR_OK) {
    Serial.printf("JPEG header failed for %s: %d\n", path.c_str(), sizeResult);
    return PictureResult::unsupported;
  }

  stopCheck = shouldStop;
  stoppedByCaller = false;
  TJpgDec.setCallback(onBlock);
  display.setSwapBytes(true);  // decoded and saved pixels are little-endian RGB565
  PictureResult result = PictureResult::shown;
  bool fits = imageWidth <= box.width && imageHeight <= box.height;
  if (fits) {
    result = drawDirect(path, imageWidth, imageHeight, box);
  } else if (!drawCachedCopy(cachePathFor(path), sourceInfo, box) &&
             !drawResized(path, imageWidth, imageHeight, box, sourceInfo, result)) {
    result = drawDirect(path, imageWidth, imageHeight, box);
  }
  display.setSwapBytes(false);
  stopCheck = nullptr;
  return result;
}
