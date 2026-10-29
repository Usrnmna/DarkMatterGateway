#pragma once

#include <cstddef>
#include <cstdint>

#include "driver/gpio.h"

namespace board {
constexpr gpio_num_t kTransmitPins[] = {GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_6};
constexpr gpio_num_t kReceive38 = GPIO_NUM_8;
constexpr gpio_num_t kReceive56 = GPIO_NUM_9;
constexpr gpio_num_t kLearnCarrier = GPIO_NUM_10;
constexpr gpio_num_t kTransmitDisable = GPIO_NUM_18; // HIGH disables buffer outputs.
constexpr gpio_num_t kCommissionButton = GPIO_NUM_14;
constexpr gpio_num_t kStatusLed = GPIO_NUM_21;
constexpr std::uint32_t kCaptureResolutionHz = 2000000;
constexpr std::uint32_t kCaptureIdleUs = 15000;
constexpr std::size_t kCaptureSymbols = 16384;
constexpr std::uint32_t kLearnTimeoutMs = 10000;

// X0A has no reviewed electrical design or pin allocation. These independent
// interlocks must remain unset until hardware qualification and board review.
constexpr bool kX0aHardwareQualified = false;
constexpr int kX0aUart = -1;
constexpr int kX0aTx = -1;
constexpr int kX0aRx = -1;
constexpr int kX0aTxInhibit = -1;
constexpr int kX0aInhibitLevel = -1;
} // namespace board
