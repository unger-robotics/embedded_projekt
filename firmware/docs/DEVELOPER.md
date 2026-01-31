# Selection Panel Firmware – Entwickler-Dokumentation

## Architektur

```
┌─────────────────────────────────────────────────────────────────────────┐
│                            main.cpp                                     │
│                     (app_loop / app_task)                               │
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
        │      drivers/         │
        │  cd4021 (Input)  ◄────│── Buttons (Active-Low)
        │  hc595  (Output) ────►│── LEDs (Active-High)
        └───────────┬───────────┘
                    ▼
        ┌───────────────────────┐
        │        hal/           │
        │      spi_bus          │
        └───────────────────────┘
```

### Layer-Verantwortlichkeiten

| Layer        | Verzeichnis     | Aufgabe                                   |
|--------------|-----------------|-------------------------------------------|
| **App**      | `src/app/`      | Orchestrierung, Serial-Kommunikation      |
| **Logic**    | `src/logic/`    | Pipeline, Debounce, Selection-Algorithmus |
| **Protocol** | `src/protocol/` | Frame-Encoding, CRC, COBS                 |
| **Drivers**  | `src/drivers/`  | Chip-spezifische Ansteuerung              |
| **HAL**      | `src/hal/`      | SPI-Bus Abstraktion                       |

## Treiber

### CD4021BE – Button-Input

Parallel-In / Serial-Out Schieberegister für Button-Matrix.

```
        CD4021BE
┌─────────────────────┐
│  P1  P2  P3  ...  P8│◄── Buttons (Active-Low, mit Pull-Up)
│                     │
│  PS ────────────────│◄── D1 (HIGH=Load, LOW=Shift)
│  CLK ───────────────│◄── D8 (SPI Clock)
│  Q8 ────────────────│──► D9 (Serial Out → MISO)
└─────────────────────┘
```

**Timing (SPI_MODE1):**

```
     PS ─────┐     ┌─────────────────────────
             └─────┘
            Load   Shift

    CLK ─────────────┐ ┐ ┐ ┐ ┐ ┐ ┐ ┐
                     └─┘─┘─┘─┘─┘─┘─┘─
                     1 2 3 4 5 6 7 8

     Q8 ═══════════╦═╦═╦═╦═╦═╦═╦═╦═══
                   ║1║2║3║4║5║6║7║8
```

Bei SPI_MODE1 (CPHA=1) shiftet die erste Taktflanke bevor SPI sampelt. Der Treiber liest Q8 einmal vor dem Clocking und aligniert den Bitstrom um 1 Bit.

```c
// C API (cd4021.h)
typedef struct cd4021_t { /* ... */ } cd4021_t;

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

### 74HC595 – LED-Output

Serial-In / Parallel-Out Schieberegister für LED-Ansteuerung.

```
        74HC595
┌─────────────────────┐
│  SER ───────────────│◄── D10 (Serial In ← MOSI)
│  SRCLK ─────────────│◄── D8  (SPI Clock)
│  RCLK ──────────────│◄── D0  (Latch)
│  OE ────────────────│◄── D2  (Output Enable, Active-Low)
│                     │
│  Q0  Q1  Q2  ...  Q7│──► LEDs (Active-High)
└─────────────────────┘
```

**Latch-Timing:**

```
   SRCLK ─┐ ┐ ┐ ┐ ┐ ┐ ┐ ┐─────────
          └─┘─┘─┘─┘─┘─┘─┘
          1 2 3 4 5 6 7 8

    RCLK ─────────────────┐   ┌───
                          └───┘
                         Latch
```

**Byte-Reihenfolge:** Bei kaskadierten Chips wird das letzte Byte zuerst gesendet (Daten "durchschieben").

```c
// C API (hc595.h)
typedef struct hc595_t { /* ... */ } hc595_t;

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

## I/O-Pipeline

Die `io_pipeline::Pipeline` orchestriert Debounce, Press-Guard und Selection:

```cpp
// logic/io_pipeline.h
namespace io_pipeline {

struct pipeline_cfg_t {
    uint32_t debounce_ms;       // Entprellzeit
    uint32_t press_guard_ms;    // Guard zwischen Druecken
    selection_policy_e policy;  // Auswahl-Policy
    uint8_t count;              // Anzahl Buttons
};

class Pipeline {
public:
    explicit Pipeline(const pipeline_cfg_t &cfg);

    // Verarbeitet raw Button-Daten, gibt true bei Aenderung zurueck
    bool update(const uint8_t *raw, size_t raw_bytes, uint32_t now_ms,
                uint8_t &out_selected);

    // Aktueller Zustand
    uint8_t selected() const;

    // Externes Setzen (Host-Preempt)
    void force_selected(uint8_t id);

private:
    pipeline_cfg_t _cfg;
    Debouncer _debouncer;
    selection_state_t _sel;
    // ...
};

}  // namespace io_pipeline
```

### Pipeline-Ablauf

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           tick() Zyklus                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  1. CD4021.read()    2. Debounce       3. Selection     4. HC595.write()│
│        │                  │                  │                 │        │
│   Raw Bits ──────► Stable Edges ──────► State ──────► LED Bits          │
│  (Active-Low)       (Bit=1 wenn        (selectedId)   (Active-High)     │
│                      neu gedrückt)                                      │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

## Debounce-Logik

### Algorithmus

Die Entprellung arbeitet sample-basiert mit konfigurierbarer Stabilisierungszeit:

```cpp
// logic/debounce.h
struct debounce_cfg_t {
    uint32_t debounce_ms;  // Stabilisierungszeit
    uint8_t count;         // Anzahl Buttons
};

class Debouncer {
public:
    explicit Debouncer(const debounce_cfg_t &cfg);

    // Neuen Sample einspeisen
    void update(const uint8_t *raw, size_t n, uint32_t now_ms);

    // Pressed-Edges abrufen (Bit=1 bei stabiler 0→1 Flanke)
    void getPressedEdges(uint8_t *out, size_t n) const;

private:
    debounce_cfg_t _cfg;
    uint8_t _last_raw_pressed[HW::BTN_BYTES];
    uint8_t _stable_pressed[HW::BTN_BYTES];
    // ...
};
```

### Zustandsmaschine pro Bit

```
                 ┌─────────────────┐
                 │                 │
                 ▼                 │ raw == last
            ┌─────────┐            │
    ────────►│  IDLE   │───────────┘
             └────┬────┘
                  │ raw != last
                  ▼
            ┌─────────┐
            │  COUNT  │◄────┐
            └────┬────┘     │ raw == pending
                 │          │ counter < threshold
                 │          │
                 ├──────────┘
                 │ counter >= threshold
                 ▼
            ┌─────────┐
            │ STABLE  │───► pressedEdge = 1 (wenn 0→1)
            └─────────┘
```

### Parameter

| Parameter     | Formel                  | Beispiel (1ms Scan, 30ms Debounce) |
|---------------|-------------------------|-------------------------------------|
| `debounce_ms` | Konfiguriert            | 30 ms                               |
| Scan-Period   | App-Konfiguration       | 1 ms                                |
| Threshold     | debounce_ms / scan_ms   | 30 Samples                          |

## Selection-Logik

### Bit-Layout (MSB-First)

IDs sind 1-basiert. Bit-Position: `mask = 1 << (7 - ((id-1) % 8))`

```
Byte 0:              Byte 1:
┌─┬─┬─┬─┬─┬─┬─┬─┐    ┌─┬─┬─┬─┬─┬─┬─┬─┐
│1│2│3│4│5│6│7│8│    │9│10│ │ │ │ │ │ │
└─┴─┴─┴─┴─┴─┴─┴─┘    └─┴──┴─┴─┴─┴─┴─┴─┘
 ▲                    ▲
Bit 7               Bit 7 (Bits 5-0 ungenutzt bei 10 Kanälen)
```

### Datentypen

```cpp
// types.h
using button_id_t = uint8_t;
constexpr button_id_t BUTTON_ID_NONE = 0;

// selection.h (C-kompatibel)
typedef struct selection_state_t {
    uint8_t selected_id;  // 0 = aus, 1–N = aktiver Kanal
} selection_state_t;

typedef enum selection_policy_e {
    SELECTION_POLICY_LOWEST_ID_WINS = 0,  // Niedrigste ID gewinnt
    SELECTION_POLICY_LAST_ID_WINS = 1,    // Hoechste ID gewinnt
} selection_policy_e;

typedef struct selection_cfg_t {
    uint8_t count;
    selection_policy_e policy;
} selection_cfg_t;
```

### API

```c
// Einmalige Auswahl aus Pressed-Edges (Bit=1 = Press-Edge)
uint8_t selection_pick(const uint8_t *edges, size_t n,
                       const selection_cfg_t *cfg);

// State aktualisieren, gibt true bei Aenderung zurueck
bool selection_update(selection_state_t *st, const uint8_t *edges,
                      size_t n, const selection_cfg_t *cfg,
                      uint8_t *out_id);
```

### Algorithmus

```cpp
// LowestIdWins: Erste gesetzte ID gewinnt
for (uint8_t id = 1; id <= cfg.count; ++id) {
    uint8_t byte = (id - 1) / 8;
    uint8_t mask = 1 << (7 - ((id - 1) % 8));
    if (pressedEdges[byte] & mask) {
        return id;  // Sofort zurück
    }
}

// LastIdWins: Letzte gesetzte ID gewinnt
uint8_t candidate = 0;
for (uint8_t id = 1; id <= cfg.count; ++id) {
    uint8_t byte = (id - 1) / 8;
    uint8_t mask = 1 << (7 - ((id - 1) % 8));
    if (pressedEdges[byte] & mask) {
        candidate = id;  // Weitermachen
    }
}
return candidate;
```

## Active-Low/High Konvertierung

```
Hardware:           Firmware:              Hardware:
Buttons             Selection              LEDs
(Active-Low)        State                  (Active-High)

  ┌───┐               ┌───┐                  ┌───┐
  │ 0 │  pressed  ──► │ 3 │  selectedId  ──► │ 1 │  LED on
  └───┘               └───┘                  └───┘
  ┌───┐                                      ┌───┐
  │ 1 │  released                            │ 0 │  LED off
  └───┘                                      └───┘

Input:  Bit=0 → Button gedrückt
Output: Bit=1 → LED an
```

## Datenfluss-Beispiel

### Button 3 gedrückt

```
1. CD4021.read(rawBits, 2)
   ┌──────────────────┐
   │ 0xDF             │  Hex: DF = 1101 1111 ← Bit 5 = 0 (Kanal 3)
   │ 0xFF             │  Hex: FF = 1111 1111
   └──────────────────┘

2. Debounce (nach 30ms stabil) → pressedEdges
   ┌──────────────────┐
   │ 0x20             │  Hex: 20 = 0010 0000 ← Bit 5 = 1 (Press-Edge)
   │ 0x00             │
   └──────────────────┘

3. selection_update(state, pressedEdges, ...)
   → state.selectedId = 3
   → return true (Änderung)

4. selectedId → LED-Bits konvertieren
   ┌──────────────────┐
   │ 0x20             │  Hex: 20 = 0010 0000 ← LED 3 an
   │ 0x00             │
   └──────────────────┘

5. HC595.write(ledBits, 2)
```

### Timing

```
t=0ms  t=10ms  t=20ms  t=30ms
  │      │       │       │
  ▼      ▼       ▼       ▼
┌────┐ ┌────┐ ┌────┐ ┌────┐ ...
│Scan│ │Scan│ │Scan│ │Scan│     (1ms Scan-Period)
└────┘ └────┘ └────┘ └────┘
  │      │       │       │
  │      └───────┴───────┘
  │         Debounce-Fenster
  │         (30 stabile Samples bei 1ms)
  ▼                      ▼
Raw=1   Raw=0   ...    Edge!
(idle)  (count)        └─► Selection
```

## Serial-Protokoll

### USB-CDC Kommunikation

| Aspekt    | Wert    | Anmerkung                                   |
|-----------|---------|---------------------------------------------|
| Interface | USB-CDC | Native USB, keine UART-Bridge               |
| Baudrate  | 921600  | Von Bridge konfiguriert (USB-CDC ignoriert) |
| Framing   | COBS    | Consistent Overhead Byte Stuffing           |
| Delimiter | `0x00`  | Frame-Ende                                  |

### SerialBridge Klasse

```cpp
// app/serial_bridge.h
typedef struct cmd_set_selection_t {
    uint16_t selected_id;  // 0 = alle aus, 1–N = Kanal
    uint8_t mode;          // 0 = normal, 1 = lock_local
    bool ack_req;          // FLAG_ACK_REQ gesetzt?
    uint16_t rx_seq;       // Fuer ACK-Antwort
} cmd_set_selection_t;

class SerialBridge {
public:
    explicit SerialBridge(Stream &s);

    // Empfang – non-blocking, gibt true bei vollstaendigem CMD zurueck
    bool poll_cmd_set_selection(cmd_set_selection_t &out);

    // Senden
    void send_hello(uint16_t btn, uint16_t led, uint16_t scan_ms,
                    uint16_t debounce_ms, uint8_t features);
    void send_ack(uint16_t seq_to_ack);
    void send_selection_state(uint16_t selected_id, uint8_t source);

private:
    Stream &_stream;
    uint16_t _tx_seq = 1;  // Auto-increment
    // ...
};
```

### Empfangene Kommandos

| Type                | Hex    | Payload | Beschreibung |
|---------------------|--------|---------|--------------|
| `CMD_SET_SELECTION` | `0x20` | 4 Bytes | Kanal setzen |

### Gesendete Nachrichten

| Type              | Hex    | Payload  | Trigger              |
|-------------------|--------|----------|----------------------|
| `HELLO`           | `0x01` | 10 Bytes | Nach Boot            |
| `SELECTION_STATE` | `0x12` | 8 Bytes  | Bei Kanaländerung    |
| `ACK`             | `0x30` | 0 Bytes  | Nach CMD mit ACK_REQ |

### Source-Werte (SELECTION_STATE.source)

| Wert | Konstante   | Bedeutung                             |
|------|-------------|---------------------------------------|
| 0    | `local_btn` | Physischer Button-Druck               |
| 1    | `host_cmd`  | Kommando vom Host (CMD_SET_SELECTION) |
| 2    | `reset`     | Nach ESP32-Reset                      |
| 3    | `recover`   | State-Recovery                        |

## FreeRTOS vs Loop

### APP_USE_FREERTOS=1 (Default)

Zwei Tasks auf separaten Cores:

```cpp
// io_task.cpp – vereinfacht
namespace app_task {

void io_task(void *) {
    io_pipeline::Pipeline pipeline(cfg);
    cd4021_t buttons; hc595_t leds;
    // ... init ...

    for (;;) {
        cd4021_read(&buttons, raw, n);
        uint8_t sel;
        if (pipeline.update(raw, n, millis(), sel)) {
            // Selection geaendert → Queue an serial_task
            xQueueSend(selectionQueue, &sel, 0);
        }
        // LED-Output
        hc595_write(&leds, led_bits, n);
        vTaskDelay(pdMS_TO_TICKS(SCAN_PERIOD_MS));
    }
}

void serial_task(void *) {
    SerialBridge bridge(Serial);
    for (;;) {
        // RX: Kommandos verarbeiten
        cmd_set_selection_t cmd;
        if (bridge.poll_cmd_set_selection(cmd)) {
            // → Pipeline setzen, ACK senden
        }

        // TX: Selection-Aenderungen senden
        uint8_t sel;
        if (xQueueReceive(selectionQueue, &sel, 0)) {
            bridge.send_selection_state(sel, SOURCE_LOCAL_BTN);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void setup() {
    xTaskCreatePinnedToCore(io_task, "io", 4096, nullptr, 1, nullptr, 0);
    xTaskCreatePinnedToCore(serial_task, "ser", 4096, nullptr, 1, nullptr, 1);
}

void loop() {
    vTaskDelay(portMAX_DELAY);  // Nicht verwendet
}

}  // namespace app_task
```

| Task        | Core | Priorität | Aufgabe                            |
|-------------|------|-----------|------------------------------------|
| io_task     | 0    | 1         | Button-Scan, Debounce, LED-Output  |
| serial_task | 1    | 1         | Serial RX/TX, Bridge-Kommunikation |

### APP_USE_FREERTOS=0

Einfache Polling-Schleife:

```cpp
// io_loop.cpp – vereinfacht
namespace app_loop {

void setup() {
    // Hardware initialisieren
    cd4021_init(&buttons, &btn_cfg);
    hc595_init(&leds, &led_cfg);
}

void loop() {
    static io_pipeline::Pipeline pipeline(cfg);
    static SerialBridge bridge(Serial);

    // 1. Button lesen + Pipeline
    cd4021_read(&buttons, raw, n);
    uint8_t sel;
    if (pipeline.update(raw, n, millis(), sel)) {
        bridge.send_selection_state(sel, SOURCE_LOCAL_BTN);
    }
    // LED-Output
    hc595_write(&leds, led_bits, n);

    // 2. Serial-Kommandos
    cmd_set_selection_t cmd;
    if (bridge.poll_cmd_set_selection(cmd)) {
        pipeline.force_selected(cmd.selected_id);
        if (cmd.ack_req) {
            bridge.send_ack(cmd.rx_seq);
        }
        bridge.send_selection_state(cmd.selected_id, SOURCE_HOST_CMD);
    }

    delay(SCAN_PERIOD_MS);
}

}  // namespace app_loop
```

## Protokoll-Details

Vollständige Protokoll-Spezifikation siehe `../pi-react/DEVELOPER.md`:

- Frame-Struktur (10-Byte Header, Payload, 2-Byte CRC)
- COBS-Encoding
- CRC-16 CCITT-FALSE (Poly: 0x1021, Init: 0xFFFF)
- Message Types und Payload-Formate

## Debugging

### Debug-Level

```ini
# platformio.ini
build_flags = -D CORE_DEBUG_LEVEL=3
```

| Level | Ausgabe  |
|-------|----------|
| 0     | Keine    |
| 1     | Errors   |
| 2     | Warnings |
| 3     | Info     |
| 4     | Debug    |
| 5     | Verbose  |

### Logic Analyzer

Empfohlene Signale für Hardware-Debugging:

| Signal | Pin | Funktion                        |
|--------|-----|---------------------------------|
| SCK    | D8  | SPI Clock – Timing verifizieren |
| MISO   | D9  | Button-Daten vom CD4021         |
| MOSI   | D10 | LED-Daten zum HC595             |
| PS     | D1  | CD4021 Load/Shift – Trigger     |
| RCK    | D0  | HC595 Latch – Datenübernahme    |

### Unit Tests isoliert ausführen

```bash
pio test -e seeed_xiao_esp32s3_test -f test_selection
pio test -e seeed_xiao_esp32s3_test -f test_debounce
pio test -e seeed_xiao_esp32s3_test -f test_io_pipeline
```
