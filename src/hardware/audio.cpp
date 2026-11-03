#include "hardware/audio.h"

#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>

#include "pins.h"

namespace {
constexpr uint32_t audioSampleRate = 16000;
constexpr i2s_port_t audioPort = I2S_NUM_0;

bool audioAvailable = false;

bool writeCodecRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(codecAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

// ES8311 as I2S slave, MCLK = 256 x sample rate, 16-bit samples, DAC to speaker output
bool initializeAudio() {
  pinMode(amplifierEnablePin, OUTPUT);
  digitalWrite(amplifierEnablePin, HIGH);

  Wire.beginTransmission(codecAddress);
  if (Wire.endTransmission() != 0) {
    Serial.println("ES8311 codec not found");
    return false;
  }

  i2s_config_t config = {};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate = audioSampleRate;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.dma_buf_count = 4;
  config.dma_buf_len = 256;
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
      {0x12, 0x00},                // power up DAC
      {0x13, 0x10},                // enable output drive
      {0x1C, 0x6A}, {0x37, 0x08},  // bypass equalizers
      {0x32, 0xA8},                // DAC volume, 0xBF = 0 dB, 0.5 dB steps
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
}  // namespace

bool beginAudio() {
  audioAvailable = initializeAudio();
  return audioAvailable;
}

// Rising C major arpeggio, each note a sine with a soft attack and exponential decay
void playStartupChime() {
  if (!audioAvailable) return;
  constexpr float notes[] = {523.25f, 659.25f, 783.99f, 1046.50f};
  constexpr float noteSeconds = 0.14f;
  constexpr float tailSeconds = 0.45f;
  constexpr float amplitude = 9000.0f;
  const uint32_t noteSamples = noteSeconds * audioSampleRate;
  const uint32_t totalSamples = noteSamples * 3 + tailSeconds * audioSampleRate;

  digitalWrite(amplifierEnablePin, LOW);
  delay(20);
  int16_t frames[256 * 2];
  uint32_t sample = 0;
  while (sample < totalSamples) {
    size_t count = 0;
    for (; count < 256 && sample < totalSamples; ++count, ++sample) {
      float value = 0;
      for (uint8_t note = 0; note < 4; ++note) {
        uint32_t start = note * noteSamples;
        if (sample < start) continue;
        float t = (sample - start) / static_cast<float>(audioSampleRate);
        float envelope = min(t / 0.005f, 1.0f) * expf(-t * (note == 3 ? 4.0f : 9.0f));
        value += envelope * sinf(2.0f * PI * notes[note] * t);
      }
      int16_t level = static_cast<int16_t>(constrain(value * amplitude, -32767.0f, 32767.0f));
      frames[count * 2] = level;
      frames[count * 2 + 1] = level;
    }
    size_t written;
    i2s_write(audioPort, frames, count * 2 * sizeof(int16_t), &written, portMAX_DELAY);
  }
  i2s_zero_dma_buffer(audioPort);
  delay(60);
  digitalWrite(amplifierEnablePin, HIGH);
}
