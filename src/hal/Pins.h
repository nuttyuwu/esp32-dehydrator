// ESP32-WROVER pin map. ARCHITECTURE.md section 2.
//
// GPIO 16 and 17 are wired to the module's PSRAM — never use them.
// GPIO 6-11 are the SPI flash, 12/15 are strapping pins, 34-39 are input-only.
#pragma once

#include <cstdint>

namespace dh {
namespace pins {

constexpr int kI2cSda = 21;
constexpr int kI2cScl = 22;

constexpr int kHeater = 27;
constexpr int kFanIntakeA = 32;
constexpr int kFanIntakeB = 33;
constexpr int kFanStack = 25;

constexpr int kKlixon = 34;   // input-only pin, external pull-up required
constexpr int kStatusLed = 2;
constexpr int kBuzzer = 4;

// Most cheap relay boards energise on a LOW input. Set this false for an SSR that
// switches on HIGH. Getting it wrong means the heater runs whenever the ESP32 is
// unprogrammed or held in reset — check it with a multimeter before wiring mains.
constexpr bool kRelayActiveLow = true;

// Klixon closed = healthy. With an external pull-up, an open thermostat reads HIGH.
constexpr bool kKlixonOpenIsHigh = true;

constexpr uint8_t kSht31AddrBottom = 0x44;
constexpr uint8_t kSht31AddrTop = 0x45;

}  // namespace pins
}  // namespace dh
