#pragma once
#include <functional>

#include "ui/ui.h"

// Draws a JPEG from the SD card centred in a box, shrunk to fit. A picture bigger than the box
// is decoded once into PSRAM, resized, and saved to /.thumbs on the card; later showings
// draw that saved copy, which takes a fraction of a second.
enum class PictureResult : uint8_t { shown, stopped, openFailed, unsupported, decodeFailed };

// Called between decoded blocks; returning true abandons the picture (used for taps)
typedef std::function<bool()> PictureStopCheck;

PictureResult drawPicture(const String &path, const Button &box, PictureStopCheck shouldStop);
