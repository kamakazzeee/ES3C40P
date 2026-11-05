#include "hardware/battery.h"

#include "pins.h"

namespace {
constexpr unsigned long presenceCheckIntervalMs = 10000;
constexpr uint8_t presenceSamples = 128;
constexpr uint32_t presenceSampleSpacingUs = 500;
constexpr float sawtoothCorrelation = 0.4f;

bool presenceChecked = false;
bool batteryFitted = true;
uint8_t disagreeingChecks = 0;
unsigned long lastPresenceCheckAt = 0;

constexpr float smoothingSeconds = 60.0f;
constexpr float longestSmoothingStepSeconds = 2.0f;  // after a pause, catch up gradually
float smoothedVoltage = 0;
bool smoothingStarted = false;
unsigned long lastSmoothedAt = 0;

// With no battery, the TP4054 charger tops its output capacitor up to 4.2 V, stops, lets it sag
// and starts again: a ~25 ms sawtooth, so readings 0.5 ms apart track each other (measured
// correlation 0.75 at 1 ms). A battery holds the voltage steady, leaving only ADC noise, which
// doesn't (measured 0.00-0.04). The average voltage is the same either way, so it can't be used.
bool measureBatteryFitted() {
  int16_t samples[presenceSamples];
  int32_t total = 0;
  unsigned long startedAt = micros();
  for (uint8_t i = 0; i < presenceSamples; ++i) {
    samples[i] = analogReadMilliVolts(batteryAdcPin);
    total += samples[i];
    while (micros() - startedAt < (i + 1) * presenceSampleSpacingUs) {
    }
  }
  float mean = total / float(presenceSamples);
  float variance = 0;
  float neighbours = 0;
  for (uint8_t i = 0; i < presenceSamples; ++i) {
    float offset = samples[i] - mean;
    variance += offset * offset;
    if (i > 0) neighbours += offset * (samples[i - 1] - mean);
  }
  if (variance == 0) return true;  // perfectly steady
  return neighbours / variance < sawtoothCorrelation;
}
}  // namespace

void beginBattery() {
  analogSetPinAttenuation(batteryAdcPin, ADC_11db);
}

float batteryVoltage() {
  uint32_t totalMillivolts = 0;
  constexpr uint8_t sampleCount = 16;
  for (uint8_t sample = 0; sample < sampleCount; ++sample) {
    totalMillivolts += analogReadMilliVolts(batteryAdcPin);
  }
  // The board halves the battery voltage before the ADC pin
  return (totalMillivolts / static_cast<float>(sampleCount)) * 2.0f / 1000.0f;
}

// A typical single-cell LiPo under light load, every 5% from empty to full. The voltage is flat
// through the middle, so a straight line from 3.3 to 4.2 V read 20-30% low there.
// To be replaced by a table measured on this board with the Battery Test screen.
uint8_t batteryPercent(float voltage) {
  constexpr float curve[] = {3.27f, 3.61f, 3.69f, 3.71f, 3.73f, 3.75f, 3.77f, 3.79f, 3.80f, 3.82f, 3.84f,
                             3.85f, 3.87f, 3.91f, 3.95f, 3.98f, 4.02f, 4.08f, 4.11f, 4.15f, 4.20f};
  constexpr uint8_t points = sizeof(curve) / sizeof(curve[0]);
  if (voltage <= curve[0]) return 0;
  if (voltage >= curve[points - 1]) return 100;
  uint8_t upper = 1;
  while (curve[upper] < voltage) ++upper;
  float between = (voltage - curve[upper - 1]) / (curve[upper] - curve[upper - 1]);
  return static_cast<uint8_t>((upper - 1 + between) * 100.0f / (points - 1) + 0.5f);
}

float smoothedBatteryVoltage() {
  float voltage = batteryVoltage();
  unsigned long now = millis();
  if (!smoothingStarted) {
    smoothedVoltage = voltage;
    smoothingStarted = true;
  } else {
    float step = min((now - lastSmoothedAt) / 1000.0f, longestSmoothingStepSeconds);
    smoothedVoltage += (voltage - smoothedVoltage) * step / (smoothingSeconds + step);
  }
  lastSmoothedAt = now;
  return smoothedVoltage;
}

bool batteryConnected() {
  if (!presenceChecked || millis() - lastPresenceCheckAt >= presenceCheckIntervalMs) {
    bool fitted = measureBatteryFitted();
    // After the first check, two in a row must agree, so one odd reading can't flip it
    disagreeingChecks = fitted == batteryFitted ? 0 : disagreeingChecks + 1;
    if (!presenceChecked || disagreeingChecks >= 2) {
      batteryFitted = fitted;
      disagreeingChecks = 0;
      smoothingStarted = false;  // a newly plugged-in battery starts from its own reading
      Serial.printf("Battery: %s\n", fitted ? "connected" : "not connected");
    }
    presenceChecked = true;
    lastPresenceCheckAt = millis();
  }
  return batteryFitted;
}
