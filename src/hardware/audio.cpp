#include "hardware/audio.h"

#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>

#include "pins.h"

namespace {
constexpr i2s_port_t audioPort = I2S_NUM_0;
constexpr size_t framesPerChunk = 256;
constexpr size_t jobChunkSamples = 512;  // 32 ms, how often the level is refreshed
constexpr BaseType_t audioCore = 0;      // loop() and all drawing run on core 1

enum class AudioJob : uint8_t { record, play, chime };

struct AudioCommand {
  AudioJob job;
  int16_t *samples;
  size_t count;
};

bool audioAvailable = false;
QueueHandle_t commandQueue = nullptr;

// Written by the audio task, read by loop()
volatile bool busy = false;
volatile bool stopRequested = false;
volatile size_t progressSamples = 0;
MicLevel latestLevel = {-100.0f, 0};
portMUX_TYPE levelLock = portMUX_INITIALIZER_UNLOCKED;

void amplifierOn() {
  digitalWrite(amplifierEnablePin, LOW);
  delay(20);
}

// Silence the DMA buffers first so the amplifier never cuts off mid-sample
void amplifierOff() {
  i2s_zero_dma_buffer(audioPort);
  delay(60);
  digitalWrite(amplifierEnablePin, HIGH);
}

bool writeCodecRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(codecAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

// ES8311 as I2S slave, MCLK = 256 x sample rate, 16-bit samples, DAC to speaker output,
// analog mic to ADC
bool initializeAudio() {
  pinMode(amplifierEnablePin, OUTPUT);
  digitalWrite(amplifierEnablePin, HIGH);

  Wire.beginTransmission(codecAddress);
  if (Wire.endTransmission() != 0) {
    Serial.println("ES8311 codec not found");
    return false;
  }

  i2s_config_t config = {};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
  config.sample_rate = audioSampleRate;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.dma_buf_count = 4;
  config.dma_buf_len = framesPerChunk;
  config.tx_desc_auto_clear = true;
  config.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  i2s_pin_config_t pins = {};
  pins.mck_io_num = i2sMasterClockPin;
  pins.bck_io_num = i2sBitClockPin;
  pins.ws_io_num = i2sWordSelectPin;
  pins.data_out_num = i2sDataOutPin;
  pins.data_in_num = i2sDataInPin;
  if (i2s_driver_install(audioPort, &config, 0, nullptr) != ESP_OK ||
      i2s_set_pin(audioPort, &pins) != ESP_OK) {
    Serial.println("I2S driver failed to start");
    return false;
  }

  const uint8_t setup[][2] = {
      {0x00, 0x1F}, {0x00, 0x00},  // reset
      {0x00, 0x80},                // power on, slave mode
      {0x01, 0x3F},                // all clocks on, MCLK from pin
      {0x02, 0x00},                // pre-divide 1, multiply 1
      {0x03, 0x10}, {0x04, 0x10},  // ADC and DAC oversampling
      {0x05, 0x00},                // ADC and DAC clock divide 1
      {0x06, 0x03}, {0x07, 0x00}, {0x08, 0xFF},
      {0x09, 0x0C}, {0x0A, 0x0C},  // I2S format, 16-bit in and out
      {0x0B, 0x00}, {0x0C, 0x00}, {0x10, 0x1F}, {0x11, 0x7F},
      {0x0D, 0x01},                // power up analog circuits
      {0x0E, 0x02},                // enable analog PGA and ADC modulator
      {0x14, 0x1A},                // analog mic on MIC1P/MIC1N, PGA +30 dB
      {0x15, 0x40},                // ADC ramp rate
      {0x16, 0x03},                // ADC digital gain +18 dB, 6 dB steps
      {0x17, 0xBF},                // ADC volume 0 dB (power-on default is muted)
      {0x12, 0x00},                // power up DAC
      {0x13, 0x10},                // enable output drive
      {0x1C, 0x6A}, {0x37, 0x08},  // bypass equalizers
      {0x32, 0xBF},                // DAC volume 0 dB, 0.5 dB steps; loudness is set per sound
      {0x31, 0x00},                // unmute DAC
  };
  for (const auto &entry : setup) {
    if (!writeCodecRegister(entry[0], entry[1])) {
      Serial.printf("ES8311 write to 0x%02X failed\n", entry[0]);
      return false;
    }
    if (entry[0] == 0x00 && entry[1] == 0x1F) delay(20);
  }
  Serial.println("ES8311 codec ready");
  return true;
}

MicLevel levelOf(const int16_t *samples, size_t count) {
  MicLevel level = {-100.0f, 0};
  if (count == 0) return level;
  int32_t sum = 0;
  for (size_t i = 0; i < count; ++i) sum += samples[i];
  float mean = sum / static_cast<float>(count);
  float squares = 0;
  for (size_t i = 0; i < count; ++i) {
    float value = samples[i] - mean;
    squares += value * value;
    level.peak = max<int16_t>(level.peak, min(abs(samples[i]), 32767));
  }
  float rms = sqrtf(squares / count);
  level.rmsDbfs = rms < 1.0f ? -100.0f : 20.0f * log10f(rms / 32768.0f);
  return level;
}

void publishLevel(const int16_t *samples, size_t count) {
  MicLevel level = levelOf(samples, count);
  portENTER_CRITICAL(&levelLock);
  latestLevel = level;
  portEXIT_CRITICAL(&levelLock);
}

// The codec fills one slot of each stereo frame, so keep whichever slot carries more signal
size_t readMono(int16_t *samples, size_t count) {
  int16_t frames[framesPerChunk * 2];
  size_t recorded = 0;
  while (recorded < count) {
    size_t wanted = min(count - recorded, framesPerChunk);
    size_t bytesRead = 0;
    i2s_read(audioPort, frames, wanted * 2 * sizeof(int16_t), &bytesRead, pdMS_TO_TICKS(100));
    size_t frameCount = bytesRead / (2 * sizeof(int16_t));
    if (frameCount == 0) break;
    uint32_t leftEnergy = 0;
    uint32_t rightEnergy = 0;
    for (size_t i = 0; i < frameCount; ++i) {
      leftEnergy += abs(frames[i * 2]);
      rightEnergy += abs(frames[i * 2 + 1]);
    }
    uint8_t slot = rightEnergy > leftEnergy ? 1 : 0;
    for (size_t i = 0; i < frameCount; ++i) samples[recorded++] = frames[i * 2 + slot];
  }
  return recorded;
}

void writeMono(const int16_t *samples, size_t count) {
  int16_t frames[framesPerChunk * 2];
  size_t played = 0;
  while (played < count) {
    size_t chunk = min(count - played, framesPerChunk);
    for (size_t i = 0; i < chunk; ++i) {
      frames[i * 2] = samples[played + i];
      frames[i * 2 + 1] = samples[played + i];
    }
    size_t written;
    i2s_write(audioPort, frames, chunk * 2 * sizeof(int16_t), &written, portMAX_DELAY);
    played += chunk;
  }
}

void recordJob(int16_t *samples, size_t count) {
  while (progressSamples < count && !stopRequested) {
    size_t got = readMono(samples + progressSamples, min(count - progressSamples, jobChunkSamples));
    if (got == 0) break;
    publishLevel(samples + progressSamples, got);
    progressSamples += got;
  }
}

void playJob(const int16_t *samples, size_t count) {
  amplifierOn();
  while (progressSamples < count && !stopRequested) {
    size_t chunk = min(count - progressSamples, jobChunkSamples);
    writeMono(samples + progressSamples, chunk);
    publishLevel(samples + progressSamples, chunk);
    progressSamples += chunk;
  }
  amplifierOff();
}

// Rising C major arpeggio, each note a sine with a soft attack and exponential decay
void chimeJob() {
  constexpr float notes[] = {523.25f, 659.25f, 783.99f, 1046.50f};
  constexpr float noteSeconds = 0.14f;
  constexpr float tailSeconds = 0.45f;
  constexpr float amplitude = 2400.0f;  // same loudness as 9000 was at the old -11.5 dB DAC volume
  const uint32_t noteSamples = noteSeconds * audioSampleRate;
  const uint32_t totalSamples = noteSamples * 3 + tailSeconds * audioSampleRate;

  amplifierOn();
  int16_t chunk[jobChunkSamples];
  uint32_t sample = 0;
  while (sample < totalSamples && !stopRequested) {
    size_t count = 0;
    for (; count < jobChunkSamples && sample < totalSamples; ++count, ++sample) {
      float value = 0;
      for (uint8_t note = 0; note < 4; ++note) {
        uint32_t start = note * noteSamples;
        if (sample < start) continue;
        float t = (sample - start) / static_cast<float>(audioSampleRate);
        float envelope = min(t / 0.005f, 1.0f) * expf(-t * (note == 3 ? 4.0f : 9.0f));
        value += envelope * sinf(2.0f * PI * notes[note] * t);
      }
      chunk[count] = static_cast<int16_t>(constrain(value * amplitude, -32767.0f, 32767.0f));
    }
    writeMono(chunk, count);
  }
  amplifierOff();
}

// Between jobs the task keeps reading the mic, which also keeps the level fresh.
// i2s_read blocks until DMA has data, so this never spins.
void audioTask(void *) {
  int16_t monitor[jobChunkSamples];
  AudioCommand command;
  for (;;) {
    if (xQueueReceive(commandQueue, &command, 0) == pdTRUE) {
      switch (command.job) {
        case AudioJob::record: recordJob(command.samples, command.count); break;
        case AudioJob::play: playJob(command.samples, command.count); break;
        case AudioJob::chime: chimeJob(); break;
      }
      busy = false;
    } else {
      publishLevel(monitor, readMono(monitor, jobChunkSamples));
    }
  }
}

// busy is set here rather than in the task so audioBusy() is true as soon as this returns
bool queueJob(AudioJob job, int16_t *samples, size_t count) {
  if (!audioAvailable || busy) return false;
  busy = true;
  progressSamples = 0;
  AudioCommand command = {job, samples, count};
  xQueueSend(commandQueue, &command, 0);
  return true;
}
}  // namespace

bool beginAudio() {
  audioAvailable = initializeAudio();
  if (!audioAvailable) return false;
  commandQueue = xQueueCreate(1, sizeof(AudioCommand));
  xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 2, nullptr, audioCore);
  return true;
}

bool audioReady() {
  return audioAvailable;
}

void playStartupChime() {
  queueJob(AudioJob::chime, nullptr, 0);
}

bool startRecording(int16_t *samples, size_t count) {
  return queueJob(AudioJob::record, samples, count);
}

bool startPlayback(const int16_t *samples, size_t count) {
  return queueJob(AudioJob::play, const_cast<int16_t *>(samples), count);
}

bool audioBusy() {
  return busy;
}

size_t audioProgress() {
  return progressSamples;
}

void stopAudio() {
  stopRequested = true;
  while (busy) delay(1);
  stopRequested = false;
}

MicLevel micLevel() {
  portENTER_CRITICAL(&levelLock);
  MicLevel level = latestLevel;
  portEXIT_CRITICAL(&levelLock);
  return level;
}
