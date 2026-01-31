# Selection Panel – ESP32-S3 Firmware

FreeRTOS-basierte Firmware für das Selection Panel mit 10 Buttons und 10 LEDs.

```
┌─────────────┐    WebSocket     ┌─────────────┐      USB-CDC       ┌─────────────┐
│   Browser   │ ◄──────────────► │  Raspberry  │ ◄────────────────► │  ESP32-S3   │
│  (React)    │      JSON        │  Pi Bridge  │   COBS + CRC16     │  Firmware   │
└─────────────┘                  └─────────────┘     921600 bd      └─────────────┘
                                                                           │
                                                                           ▼
                                                                    ┌─────────────┐
                                                                    │  Shift-     │
                                                                    │  Register   │
                                                                    │ CD4021/595  │
                                                                    └─────────────┘
```

## Schnellstart

### Build & Flash

```bash
cd firmware

# Build (FreeRTOS-Variante, default)
pio run

# Flash
pio run -t upload

# Serial Monitor (Debug-Output)
pio device monitor
```

### Alternative: Loop-Variante (ohne FreeRTOS)

```bash
pio run -e seeed_xiao_esp32s3_loop
pio run -e seeed_xiao_esp32s3_loop -t upload
```

### Tools

```bash
./tools/format.sh       # Code formatieren (clang-format)
./tools/lint.sh         # Statische Analyse (cppcheck)
```

## Hardware

### Board

**Seeed XIAO ESP32-S3** – Kompaktes Modul mit USB-C und nativem USB-CDC.

### Pin-Belegung

```
XIAO ESP32-S3
┌─────────────────────────────────┐
│  D0  ●────────────────► LED_RCK │  74HC595 Latch
│  D1  ●────────────────► BTN_PS  │  CD4021BE Parallel/Shift
│  D2  ●────────────────► LED_OE  │  74HC595 Output Enable
│  D8  ●────────────────► SCK     │  SPI Clock (gemeinsam)
│  D9  ●◄───────────────  BTN_MISO│  CD4021BE Q8
│  D10 ●────────────────► LED_MOSI│  74HC595 SER
└─────────────────────────────────┘
```

| Pin | Signal   | Chip     | Funktion                   |
|-----|----------|----------|----------------------------|
| D0  | LED_RCK  | 74HC595  | Latch (RCLK)               |
| D1  | BTN_PS   | CD4021BE | Parallel/Shift             |
| D2  | LED_OE   | 74HC595  | Output Enable (Active-Low) |
| D8  | SCK      | Beide    | SPI Clock                  |
| D9  | BTN_MISO | CD4021BE | Serial Out (Q8)            |
| D10 | LED_MOSI | 74HC595  | Serial In (SER)            |

### Schieberegister

| Chip     | Anzahl | Funktion                | SPI            |
|----------|--------|-------------------------|----------------|
| CD4021BE | 2      | 16 Inputs (10 genutzt)  | 500 kHz, MODE1 |
| 74HC595  | 2      | 16 Outputs (10 genutzt) | 1 MHz, MODE0   |

## Konfiguration

### Build-Varianten

| Environment                   | Beschreibung                 |
|-------------------------------|------------------------------|
| `seeed_xiao_esp32s3_freertos` | FreeRTOS Dual-Core (default) |
| `seeed_xiao_esp32s3_loop`     | Single-Core Polling          |

### Parameter (io_counts.h)

```cpp
constexpr uint8_t BTN_COUNT = 10;  // Anzahl Buttons
constexpr uint8_t LED_COUNT = 10;  // Anzahl LEDs
```

### Timing

| Parameter   | Default | Datei                     | Beschreibung             |
|-------------|---------|---------------------------|--------------------------|
| Scan-Period | 1 ms    | io_loop.cpp / io_task.cpp | Button-Abfrage-Intervall |
| Debounce    | 30 ms   | io_loop.cpp / io_task.cpp | Entprellzeit             |

## Verzeichnisstruktur

```
firmware/
├── include/
│   ├── hw/
│   │   ├── board_pins.h     # GPIO-Zuordnung
│   │   ├── io_counts.h      # Button/LED-Anzahl
│   │   ├── spi_params.h     # SPI-Konfiguration
│   │   └── timing_params.h  # Treiber-Timing-Konstanten
│   ├── error_codes.h        # Zentrale Fehlercodes
│   └── types.h              # button_id_t
├── src/
│   ├── app/                 # Applikationsschicht
│   │   ├── serial_bridge.*  # Host-Kommunikation
│   │   ├── io_loop.cpp      # Single-Core Variante
│   │   └── io_task.cpp      # FreeRTOS Variante
│   ├── drivers/             # Hardware-Treiber (C API)
│   │   ├── cd4021.*         # Button-Input
│   │   └── hc595.*          # LED-Output
│   ├── hal/                 # SPI-Abstraktion (C API)
│   │   └── spi_bus.*
│   ├── logic/               # Geschaeftslogik
│   │   ├── debounce.*       # Entprellung
│   │   ├── selection.*      # Kanalauswahl
│   │   └── io_pipeline.*    # Orchestrierung
│   ├── protocol/            # Kommunikation
│   │   └── frames.*         # COBS, CRC, Frames
│   └── main.cpp             # Entry Point
├── test/                    # Unit Tests
├── tools/                   # Format/Lint Scripts
└── platformio.ini           # Build-Konfiguration
```

## Tests & Qualitaet

```bash
# Unit Tests (Logic only)
pio test -e seeed_xiao_esp32s3_test

# End-to-End mit Hardware
pio test -e seeed_xiao_esp32s3_e2e

# Statische Analyse
./tools/lint.sh

# Code formatieren
./tools/format.sh
```

| Test               | Beschreibung              |
|--------------------|---------------------------|
| `test_debounce`    | Entprelllogik             |
| `test_selection`   | Kanalverwaltung           |
| `test_io_pipeline` | Scan → Selection Pipeline |
| `test_e2e_hw`      | Hardware-Integration      |


### Baudraten-Hinweis

| Zweck            | Baudrate | Anmerkung                         |
|------------------|----------|-----------------------------------|
| Debug-Monitor    | 115200   | `pio device monitor`              |
| Bridge-Protokoll | 921600   | USB-CDC (Baudrate wird ignoriert) |


