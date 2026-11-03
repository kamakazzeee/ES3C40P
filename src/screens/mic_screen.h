#pragma once
#include "screens/screen.h"

class MicScreen : public Screen {
 public:
  void enter() override;
  void draw() override;
  void onTap(uint16_t x, uint16_t y) override;
  void update() override;
  void leave() override;

 private:
  void drawMeter(float rmsDbfs, int16_t peak);
  void drawControls(const char *recordLabel, const char *playLabel);
  void finishRecording();
  int16_t *clip = nullptr;
  size_t clipLength = 0;
  bool recording = false;
  bool playing = false;
  unsigned long lastMeterAt = 0;
};

extern MicScreen micScreen;
