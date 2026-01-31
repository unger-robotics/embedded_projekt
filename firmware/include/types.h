#pragma once
#include <cstdint>

// IDs sind 1-basiert: 1..N, 0 bedeutet "none".
using button_id_t = uint8_t;
constexpr button_id_t BUTTON_ID_NONE = 0;
