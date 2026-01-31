#pragma once
#include <Arduino.h>
#include <SPI.h>

namespace HW {

// Frequenzen/Modi (SSOT)
constexpr uint32_t SPI_4021_HZ = 500000UL;
constexpr uint8_t SPI_4021_MODE = SPI_MODE1;

constexpr uint32_t SPI_595_HZ = 1000000UL;
constexpr uint8_t SPI_595_MODE = SPI_MODE0;

// Optional: Bitorder als Zahltyp (ESP32-Core hat keinen BitOrder-Typ)
constexpr uint8_t SPI_4021_BITORDER = MSBFIRST;
constexpr uint8_t SPI_595_BITORDER = MSBFIRST;

} // namespace HW
