#pragma once
#include <stdint.h>
namespace hardware {
// Preliminary classic ESP32 GPIO map; MATRIX must not drive long GPIO cables.
constexpr uint8_t moisturePins[4] = {32, 33, 34, 35};
constexpr uint8_t temperaturePin = 18;
constexpr uint8_t relayPins[4] = {25, 26, 27, 23};
// No populated sensors by default. Bits 0..3 correspond to moisturePins.
constexpr uint8_t moistureMask = 0;
constexpr bool temperatureEnabled = false;
constexpr uint32_t sampleIntervalMs = 10000;
static_assert((moistureMask & 0xf0) == 0, "Maximum four moisture sensors");
} // namespace hardware
