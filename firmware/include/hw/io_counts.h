#pragma once
#include <cstddef>
#include <cstdint>

namespace HW {

constexpr uint8_t BTN_COUNT = 10;
constexpr uint8_t LED_COUNT = 10;

constexpr size_t BTN_BYTES = (BTN_COUNT + 7) / 8;
constexpr size_t LED_BYTES = (LED_COUNT + 7) / 8;

// Buttons: MSB-first -> letzte Byte-Maske nutzt die oberen Bits
// BTN_COUNT=10 => 2 Bits im letzten Byte: bits7..6 => 0xC0
constexpr uint8_t BTN_LAST_MASK =
    (BTN_COUNT % 8 == 0)
        ? 0xFFu
        : static_cast<uint8_t>(0xFFu << (8u - (BTN_COUNT % 8u)));

constexpr uint8_t LED_LAST_MASK =
    (LED_COUNT % 8 == 0) ? 0xFFu
                         : static_cast<uint8_t>((1u << (LED_COUNT % 8u)) - 1u);

} // namespace HW
