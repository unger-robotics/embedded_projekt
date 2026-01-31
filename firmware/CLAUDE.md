# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
pio run                              # Build (FreeRTOS-Variante, default)
pio run -e seeed_xiao_esp32s3_loop   # Build Loop-Variante (ohne FreeRTOS)
pio run -t upload                    # Build and flash
pio run -t upload && pio device monitor  # Flash and monitor
pio device monitor                   # Serial monitor (115200 baud)
```

## Tests

```bash
pio test -e seeed_xiao_esp32s3_test              # Unit tests (nur Logic)
pio test -e seeed_xiao_esp32s3_test -f test_selection  # Einzelner Test
pio test -e seeed_xiao_esp32s3_test -f test_debounce   # Debounce Test
pio test -e seeed_xiao_esp32s3_test -f test_io_pipeline # Pipeline Test
pio test -e seeed_xiao_esp32s3_e2e               # E2E mit Hardware
```

## Linting & Formatierung

```bash
./tools/format.sh    # clang-format (vor Commit)
./tools/lint.sh      # cppcheck (0 HIGH/MEDIUM warnings erwartet)
```

## Architecture

**Target**: ESP32-S3 (Seeed XIAO ESP32S3) mit Arduino-Framework

```
src/app/      → Applikationsschicht (io_task/io_loop, serial_bridge)
src/logic/    → Geschaeftslogik (debounce, selection, io_pipeline)
src/drivers/  → Chip-Treiber (cd4021 Buttons, hc595 LEDs) - C API
src/hal/      → HAL (spi_bus) - C API mit C++ Wrapper
src/protocol/ → Protokoll (frames mit COBS/CRC)
include/hw/   → Hardware-Konfiguration (SSOT fuer Pins, Counts, SPI, Timing)
include/      → Typen (error_codes.h)
```

**Entry Point**: `src/main.cpp` dispatcht zu `app_task` (FreeRTOS) oder `app_loop` (Polling) je nach `APP_USE_FREERTOS`

**Build-Varianten**:
- `APP_USE_FREERTOS=1` (default): Dual-Core mit io_task (Core 0) und serial_task (Core 1)
- `APP_USE_FREERTOS=0`: Single-threaded Polling in Arduino loop()

**Wichtigste Regel**: Logic-Schicht (`src/logic/`) ist hardware-unabhaengig und testbar ohne Hardware. Nur Drivers/HAL greifen auf Arduino/SPI zu.

## Datenfluss (pro Tick)

```
cd4021_read() → Debounce::update() → selection_update() → hc595_write()
   rawBits        pressedEdges          selectedId          ledBits
 (Active-Low)    (Edge=1 bei Press)    (0=keiner, 1-10)   (Active-High)
```

## Bit-Mapping (MSB-First)

IDs 1-10 mappen auf Bits mit MSB = ID 1. Formel: `mask = 1 << (7 - ((id-1) % 8))`

```
Byte 0: [1][2][3][4][5][6][7][8]
Byte 1: [9][10][x][x][x][x][x][x]
```

## Hardware-Konfiguration (SSOT)

| Datei | Inhalt |
|-------|--------|
| `include/hw/board_pins.h` | GPIO-Zuordnung (D0-D10) |
| `include/hw/io_counts.h` | BTN_COUNT, LED_COUNT, Masken |
| `include/hw/spi_params.h` | SPI-Frequenzen (CD4021: 500kHz MODE1, HC595: 1MHz MODE0) |
| `include/hw/timing_params.h` | Timing-Konstanten (PS Setup/Hold, Latch Pulse) |

## Serial-Protokoll

USB-CDC mit COBS-Framing, 0x00 Delimiter, CRC-16 CCITT-FALSE (921600 baud).

| Message | Hex | Beschreibung |
|---------|-----|--------------|
| MSG_TYPE_HELLO | 0x01 | Nach Boot mit Config |
| MSG_TYPE_SELECTION_STATE | 0x12 | Bei Kanaländerung |
| MSG_TYPE_CMD_SET_SELECTION | 0x20 | Kanal setzen (empfangen) |
| MSG_TYPE_ACK | 0x30 | Antwort bei ACK_REQ Flag |

## Coding Standard

**Basis**: BARR-C (Style/Lesbarkeit) + MISRA-orientierte Regeln

**Sprachsplit**: C fuer Treiber/HAL (mit C++ Wrapper), C++ fuer Applikation/Architektur

### Naming

| Element | Style | Beispiel |
|---------|-------|----------|
| Structs | snake_case + `_t` | `selection_state_t`, `cd4021_t` |
| Enums | snake_case + `_e` | `selection_policy_e`, `msg_type_e` |
| Enum-Werte | UPPER_SNAKE_CASE | `SELECTION_POLICY_LOWEST_ID_WINS` |
| Klassen | PascalCase | `SerialBridge`, `Debouncer` |
| C-Funktionen | prefix_snake | `cd4021_read`, `spi_bus_init` |
| Private Member | `_` prefix | `_bus`, `_cfg`, `_stream` |

### Regeln

- **Keine Magic Numbers**: Pins/Zeiten in Config-Headers
- **Keine impliziten Casts**: `stdint.h`-Typen bevorzugen
- **C++ Embedded-Subset**: RAII ja, Exceptions nein; Fehler ueber Return-Code

### Error Handling

`error_code_e` aus `include/error_codes.h`. Return `APP_ERR_SUCCESS` (0) bei Erfolg, negative Werte bei Fehler.

### C/C++ Interoperability

Alle C-Header brauchen `extern "C"` Guards:
```c
#ifdef __cplusplus
extern "C" {
#endif
// declarations
#ifdef __cplusplus
}
#endif
```

C-Treiber (cd4021, hc595, spi_bus) bieten optionale C++ Wrapper-Klassen.

## Dokumentation

Detaillierte Dokumentation in `docs/`:
- `architecture.md` – Schichtenmodell, Datenfluss
- `DEVELOPER.md` – API-Dokumentation
- `ALGORITHM.md` – Algorithmen (Debounce, Selection, etc.)
- `FLOWCHART.md` – Visuelle Ablaufdiagramme
- `HARDWARE.md` – Pin-Belegung, Schieberegister
- `CODING_STANDARD.md` – Vollstaendiger Coding Standard
