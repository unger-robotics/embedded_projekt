#pragma once
#include "types.h"

// Event-Semantik: neue Auswahl (Selection-Change), nicht "raw pressed".
struct IoEvent {
    button_id_t selectedId = BUTTON_ID_NONE;
};
