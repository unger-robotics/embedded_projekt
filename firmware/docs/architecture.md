# Firmware-Architektur

Schichtenmodell und Datenfluss des Selection Panel Firmware.

---

## 1) Schichtenmodell

```
┌─────────────────────────────────────────────────────────────────────────┐
│                            main.cpp                                      │
│                     (app_loop / app_task)                                │
└───────────────────────────────┬─────────────────────────────────────────┘
                                │
        ┌───────────────────────┼───────────────────────┐
        ▼                       ▼                       ▼
┌───────────────┐       ┌───────────────┐       ┌───────────────┐
│   app/        │       │   logic/      │       │   protocol/   │
│ serial_bridge │       │  io_pipeline  │       │    frames     │
│   io_loop     │       │  selection    │       │  (COBS/CRC)   │
│   io_task     │       │  debounce     │       │               │
└───────┬───────┘       └───────┬───────┘       └───────────────┘
        │                       │
        └───────────┬───────────┘
                    ▼
        ┌───────────────────────┐
        │    drivers/ (C API)   │
        │  cd4021 (Input)  ◄────│── Buttons (Active-Low)
        │  hc595  (Output) ────►│── LEDs (Active-High)
        └───────────┬───────────┘
                    ▼
        ┌───────────────────────┐
        │     hal/ (C API)      │
        │      spi_bus          │
        └───────────┬───────────┘
                    ▼
        ┌───────────────────────┐
        │   Arduino / ESP-IDF   │
        │   (SPI, GPIO, etc.)   │
        └───────────────────────┘
```

### Layer-Verantwortlichkeiten

| Layer        | Verzeichnis     | Sprache | Aufgabe                                   |
|--------------|-----------------|---------|-------------------------------------------|
| **App**      | `src/app/`      | C++     | Orchestrierung, Serial-Kommunikation      |
| **Logic**    | `src/logic/`    | C++     | Pipeline, Debounce, Selection-Algorithmus |
| **Protocol** | `src/protocol/` | C++     | Frame-Encoding, CRC, COBS                 |
| **Drivers**  | `src/drivers/`  | C API   | Chip-spezifische Ansteuerung              |
| **HAL**      | `src/hal/`      | C API   | SPI-Bus Abstraktion                       |

**Prinzip:** Treiber sind "dumm" (nur Hardware-Zugriff), Regeln gehoeren in `logic/`.

---

## 2) Datenfluss (ein Zyklus)

```
cd4021_read()
      │
      ▼ raw[] (Active-Low, MSB-first)
┌─────────────────────────────────────────┐
│              io_pipeline                 │
│  ┌─────────┐  ┌────────────┐  ┌────────┐│
│  │Debouncer│─►│Press-Guard │─►│Selection││
│  │(Edges)  │  │(optional)  │  │(Policy)││
│  └─────────┘  └────────────┘  └────────┘│
└─────────────────────────────────────────┘
      │
      ▼ selected_id (0..N)
set_one_hot() ─► led_state[]
      │
      ▼
hc595_write()
```

### Detaillierter Ablauf

1. `cd4021_read(raw)` – Liest Button-Zustaende (Active-Low)
2. `pipeline.update(raw, now_ms, &selected)`:
   - Debounce → Press-Edges (stabile Flanken)
   - Press-Guard → Sperrt schnelle Re-Presses (optional)
   - Selection → `selected_id` nach Policy
3. `set_one_hot(selected_id)` → `led_state[]` (maskiert)
4. `hc595_write(led_state)` → LED-Ausgaenge

---

## 3) Treiber-API (C Interface)

### CD4021 (Button-Input)

```c
typedef struct cd4021_t { /* opak */ } cd4021_t;

typedef struct cd4021_cfg_t {
    spi_bus_t *bus;
    spi_settings_t spi;
    int pin_ps;
    int pin_miso;
} cd4021_cfg_t;

void cd4021_init(cd4021_t *dev, const cd4021_cfg_t *cfg);
void cd4021_begin(cd4021_t *dev);
void cd4021_read(cd4021_t *dev, uint8_t *out, size_t n);
```

### HC595 (LED-Output)

```c
typedef struct hc595_t { /* opak */ } hc595_t;

typedef struct hc595_cfg_t {
    spi_bus_t *bus;
    spi_settings_t spi;
    int pin_rck;
    int pin_oe;
} hc595_cfg_t;

void hc595_init(hc595_t *dev, const hc595_cfg_t *cfg);
void hc595_begin(hc595_t *dev, bool enable_outputs);
void hc595_write(hc595_t *dev, const uint8_t *data, size_t n);
```

### SPI-Bus (HAL)

```c
typedef struct spi_bus_t { /* opak */ } spi_bus_t;

typedef struct spi_settings_t {
    uint32_t freq_hz;
    uint8_t bit_order;  // MSBFIRST
    uint8_t mode;       // SPI_MODE0..3
} spi_settings_t;

void spi_bus_begin(spi_bus_t *bus, int sck, int miso, int mosi);
void spi_bus_transfer_full(spi_bus_t *bus, const spi_settings_t *s,
                           const uint8_t *tx, uint8_t *rx, size_t n);
```

---

## 4) Logic-API

### Pipeline

```cpp
namespace io_pipeline {

struct pipeline_cfg_t {
    uint32_t debounce_ms;       // 30 ms
    uint32_t press_guard_ms;    // 30 ms
    selection_policy_e policy;  // LOWEST_ID_WINS
    uint8_t count;              // 10
};

class Pipeline {
public:
    explicit Pipeline(const pipeline_cfg_t &cfg);

    bool update(const uint8_t *raw, size_t raw_bytes,
                uint32_t now_ms, uint8_t &out_selected);

    uint8_t selected() const;
    void force_selected(uint8_t id);

private:
    pipeline_cfg_t _cfg;
    Debouncer _debouncer;
    selection_state_t _sel;
    // ...
};

}  // namespace io_pipeline
```

### Selection

```c
typedef enum selection_policy_e {
    SELECTION_POLICY_LOWEST_ID_WINS = 0,
    SELECTION_POLICY_LAST_ID_WINS = 1,
} selection_policy_e;

typedef struct selection_state_t {
    uint8_t selected_id;
} selection_state_t;

typedef struct selection_cfg_t {
    uint8_t count;
    selection_policy_e policy;
} selection_cfg_t;

bool selection_update(selection_state_t *st, const uint8_t *edges,
                      size_t n, const selection_cfg_t *cfg,
                      uint8_t *out_id);
```

---

## 5) Invarianten

| Invariante | Beschreibung |
|------------|--------------|
| **One-Hot** | Maximal eine LED gesetzt, oder keine |
| **Invalid Bits** | Ungenutzte Bits im letzten Byte werden mit `LED_LAST_MASK` maskiert |
| **IDs 1-basiert** | `selected_id ∈ {0, 1..count}`, 0 = keine Auswahl |

---

## 6) SPI-Parameter (SSOT)

Aus `include/hw/spi_params.h`:

| Device  | Frequenz | Mode       | Anmerkung                  |
|---------|----------|------------|----------------------------|
| CD4021  | 500 kHz  | `SPI_MODE1`| Konservativ fuer Breadboard |
| 74HC595 | 1 MHz    | `SPI_MODE0`| Standard                   |

Beide Devices teilen sich SCK/MOSI/MISO, aber Transfers laufen mit eigenen `spi_settings_t`.

---

## 7) Build-Varianten

### Firmware

| Environment                       | Beschreibung                         |
|-----------------------------------|--------------------------------------|
| `seeed_xiao_esp32s3_freertos`     | FreeRTOS Dual-Core (APP_USE_FREERTOS=1) |
| `seeed_xiao_esp32s3_loop`         | Arduino Loop (APP_USE_FREERTOS=0)    |

### Tests

| Environment                       | Beschreibung                    |
|-----------------------------------|---------------------------------|
| `seeed_xiao_esp32s3_test`         | Unit-Tests (logic-only)         |
| `seeed_xiao_esp32s3_e2e`          | E2E-HW (logic+drivers+hal)      |

---

## 8) Verzeichnisstruktur

```
firmware/
├── include/
│   ├── hw/
│   │   ├── board_pins.h      # Pin-Definitionen (SSOT)
│   │   ├── io_counts.h       # Counts, Masks (SSOT)
│   │   ├── spi_params.h      # SPI-Parameter (SSOT)
│   │   └── timing_params.h   # Treiber-Timing-Konstanten
│   ├── error_codes.h         # Zentrale Fehlercodes (error_code_e)
│   └── types.h               # button_id_t
├── src/
│   ├── hal/
│   │   └── spi_bus.cpp       # SPI-Initialisierung (C API)
│   ├── drivers/
│   │   ├── cd4021.cpp        # Button-Input (C API)
│   │   └── hc595.cpp         # LED-Output (C API)
│   ├── logic/
│   │   ├── debounce.cpp      # Entprellung
│   │   ├── selection.cpp     # Selection-Policy
│   │   └── io_pipeline.cpp   # Orchestrierung
│   ├── app/
│   │   ├── io_loop.cpp       # Arduino-Variante
│   │   ├── io_task.cpp       # FreeRTOS-Variante
│   │   └── serial_bridge.cpp # Host-Kommunikation
│   └── protocol/
│       └── frames.cpp        # COBS/CRC Frames
├── test/
│   ├── test_debounce/
│   ├── test_selection/
│   ├── test_io_pipeline/
│   └── test_e2e_hw/
├── tools/
│   ├── format.sh             # clang-format
│   └── lint.sh               # cppcheck
└── platformio.ini
```

---

## 9) FreeRTOS vs Loop

### APP_USE_FREERTOS=1 (Default)

Zwei Tasks auf separaten Cores:

| Task        | Core | Prioritaet | Aufgabe                            |
|-------------|------|------------|------------------------------------|
| io_task     | 0    | 1          | Button-Scan, Debounce, LED-Output  |
| serial_task | 1    | 1          | Serial RX/TX, Bridge-Kommunikation |

Kommunikation ueber `xQueue` (selectionQueue).

### APP_USE_FREERTOS=0

Einfache Polling-Schleife in `loop()`:
- I/O-Zyklus (read → pipeline → write)
- Serial-Kommandos (poll → process → respond)
- 1ms Delay

---

## 10) Serial-Protokoll

| Aspekt    | Wert    | Anmerkung                    |
|-----------|---------|------------------------------|
| Interface | USB-CDC | Native USB, keine UART-Bridge|
| Baudrate  | 921600  | Bridge-Protokoll             |
| Framing   | COBS    | 0x00 als Delimiter           |
| CRC       | CRC-16  | CCITT-FALSE                  |

### Message Types

| Type              | Hex    | Richtung | Beschreibung         |
|-------------------|--------|----------|----------------------|
| HELLO             | 0x01   | TX       | Nach Boot            |
| SELECTION_STATE   | 0x12   | TX       | Bei Kanaländerung    |
| CMD_SET_SELECTION | 0x20   | RX       | Kanal setzen         |
| ACK               | 0x30   | TX       | Antwort auf CMD      |

---

## Siehe auch

- `DEVELOPER.md` – Detaillierte API-Dokumentation
- `FLOWCHART.md` – Visuelle Ablaufdiagramme
- `ALGORITHM.md` – Algorithmen-Details
- `HARDWARE.md` – Pin-Belegung und Schieberegister
- `CODING_STANDARD.md` – Naming Conventions
