#pragma once
#include <Arduino.h>

// ES8311 codec + FM8002E speaker amplifier, and the codec's microphone input.
// Audio runs in its own task on core 0: these calls queue a job and return at once,
// and the caller polls audioBusy(). Only one job runs at a time.
constexpr uint32_t audioSampleRate = 16000;

struct MicLevel {
  float rmsDbfs;  // -100 when nothing was read
  int16_t peak;
};

bool beginAudio();  // Wire must already be started
bool audioReady();
void playStartupChime();
void playKeyClick();  // skipped if another sound is playing

// Mono 16-bit samples at audioSampleRate. The buffer must stay valid until the job
// finishes or stopAudio() returns. Both return false if audio is missing or busy.
bool startRecording(int16_t *samples, size_t count);
bool startPlayback(const int16_t *samples, size_t count);

bool audioBusy();
size_t audioProgress();  // samples recorded or played by the current or last job
void stopAudio();        // ends the current job and waits for it

// Level of the latest ~32 ms heard by the mic, or of the sound being played; DC removed
MicLevel micLevel();
