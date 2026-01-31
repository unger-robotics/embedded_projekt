#pragma once
#include <Arduino.h>

namespace HW {

// XIAO ESP32-S3 (Beispiel-Pins; passe an deine Verdrahtung an)
constexpr int PIN_SCK = D8;      // SPI SCK (gemeinsam)
constexpr int PIN_BTN_PS = D1;   // CD4021: Parallel/Shift (PS)
constexpr int PIN_BTN_MISO = D9; // CD4021: Q8 -> MISO

constexpr int PIN_LED_MOSI = D10; // 74HC595: SER -> MOSI
constexpr int PIN_LED_RCK = D0;   // 74HC595: RCK/Latch
constexpr int PIN_LED_OE = D2;    // 74HC595: OE (active-low), optional

} // namespace HW
