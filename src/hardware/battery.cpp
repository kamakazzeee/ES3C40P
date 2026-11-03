#include "hardware/battery.h"

#include "pins.h"

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

uint8_t batteryPercent(float voltage) {
  constexpr float minVoltage = 3.30f;
  constexpr float maxVoltage = 4.20f;
  if (voltage <= minVoltage) return 0;
  if (voltage >= maxVoltage) return 100;
  return static_cast<uint8_t>((voltage - minVoltage) / (maxVoltage - minVoltage) * 100.0f);
}

String batteryStatusText() {
  float voltage = batteryVoltage();
  return String(batteryPercent(voltage)) + "% " + String(voltage, 2) + "V";
}
