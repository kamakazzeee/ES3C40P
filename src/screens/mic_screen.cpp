#include "screens/mic_screen.h"

#include "app.h"
#include "hardware/audio.h"
#include "ui/ui.h"

MicScreen micScreen;

namespace {
constexpr unsigned long meterIntervalMs = 50;
constexpr float clipSeconds = 3.0f;
constexpr size_t clipCapacity = clipSeconds * audioSampleRate;
constexpr float meterFloorDb = -70.0f;
constexpr float maxPlaybackGain = 16.0f;

constexpr Button meterBox = {18, 116, 444, 36};
constexpr Button recordButton = {18, 196, 216, 52};
constexpr Button playButton = {246, 196, 216, 52};
}  // namespace

// 3 s of 16-bit audio is 96 KB, so the clip lives in PSRAM and only while this screen is open
void MicScreen::enter() {
  clip = static_cast<int16_t *>(ps_malloc(clipCapacity * sizeof(int16_t)));
  if (!clip) clip = static_cast<int16_t *>(malloc(clipCapacity * sizeof(int16_t)));
  clipLength = 0;
}

// The audio task may still be filling or reading the clip
void MicScreen::leave() {
  stopAudio();
  free(clip);
  clip = nullptr;
  clipLength = 0;
  recording = false;
  playing = false;
}

void MicScreen::draw() {
  drawHeader("MICROPHONE TEST");
  if (!audioReady()) {
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.drawCentreString("AUDIO CODEC NOT DETECTED", 240, 145, 4);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawCentreString("ES8311 did not answer at 0x18", 240, 188, 2);
  } else {
    display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    display.drawCentreString("Speak or clap near the board", 240, 92, 2);
    drawMeter(-100.0f, 0);
    drawControls("RECORD 3 S", "PLAY BACK");
  }
  drawBackButton();
}

void MicScreen::onTap(uint16_t x, uint16_t y) {
  if (backButtonTapArea.contains(x, y)) {
    showMenu();
  } else if (!audioReady() || recording || playing) {
    return;
  } else if (clip && recordButton.contains(x, y)) {
    clipLength = 0;
    if (!startRecording(clip, clipCapacity)) return;
    recording = true;
    drawControls("RECORDING...", "PLAY BACK");
  } else if (clipLength > 0 && playButton.contains(x, y)) {
    if (!startPlayback(clip, clipLength)) return;
    playing = true;
    drawControls("RECORD 3 S", "PLAYING...");
  }
}

void MicScreen::update() {
  if (!audioReady()) return;
  if (recording && !audioBusy()) finishRecording();
  if (playing && !audioBusy()) {
    playing = false;
    drawControls("RECORD 3 S", "PLAY BACK");
  }
  if (millis() - lastMeterAt < meterIntervalMs) return;
  lastMeterAt = millis();
  MicLevel level = micLevel();
  drawMeter(level.rmsDbfs, level.peak);
}

// Green while quiet, yellow when loud, red near clipping
void MicScreen::drawMeter(float rmsDbfs, int16_t peak) {
  float fraction = constrain((rmsDbfs - meterFloorDb) / -meterFloorDb, 0.0f, 1.0f);
  int16_t innerWidth = meterBox.width - 4;
  int16_t filled = fraction * innerWidth;
  uint16_t color = rmsDbfs > -6.0f ? TFT_RED : rmsDbfs > -20.0f ? TFT_YELLOW : TFT_GREEN;
  display.drawRect(meterBox.left, meterBox.top, meterBox.width, meterBox.height, TFT_LIGHTGREY);
  display.fillRect(meterBox.left + 2, meterBox.top + 2, filled, meterBox.height - 4, color);
  display.fillRect(meterBox.left + 2 + filled, meterBox.top + 2, innerWidth - filled,
                   meterBox.height - 4, TFT_BLACK);

  String levelText = rmsDbfs <= -100.0f ? String("Level: -- dBFS") : "Level: " + String(rmsDbfs, 1) + " dBFS";
  display.fillRect(18, 160, 444, 20, TFT_BLACK);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString(levelText, 18, 162, 2);
  display.drawRightString("Peak: " + String(peak), 462, 162, 2);
}

void MicScreen::drawControls(const char *recordLabel, const char *playLabel) {
  drawButton(recordButton, recordLabel, TFT_MAROON, TFT_RED, TFT_WHITE);
  uint16_t playText = clipLength > 0 ? TFT_WHITE : TFT_DARKGREY;
  drawButton(playButton, playLabel, TFT_DARKGREEN, TFT_GREEN, playText);
}

void MicScreen::finishRecording() {
  recording = false;
  clipLength = audioProgress();

  // Scale the clip so its peak is near full volume; the cap keeps a silent clip from
  // turning into loud hiss
  int32_t peak = 1;
  for (size_t i = 0; i < clipLength; ++i) peak = max<int32_t>(peak, abs(clip[i]));
  float gain = min(30000.0f / peak, maxPlaybackGain);
  for (size_t i = 0; i < clipLength; ++i) clip[i] = clip[i] * gain;
  Serial.printf("Mic: recorded %u samples, peak %d, playback gain %.1fx\n",
                static_cast<unsigned>(clipLength), static_cast<int>(peak), gain);
  drawControls("RECORD 3 S", "PLAY BACK");
}
