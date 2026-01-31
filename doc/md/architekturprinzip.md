# Architekturprinzipien für robuste Embedded-Software

## 0) Begriffsrahmen und Zielsetzung

### Anwendungskontext

Diese Dokumentation richtet sich an Embedded-Entwickler, die ein Selection-Panel mit 10–100 Buttons und LEDs über Schieberegister (CD4021B, 74HC595) steuern. Die Firmware läuft auf einem ESP32-S3 unter FreeRTOS. Die Architekturprinzipien gelten jedoch allgemein für IO-intensive Embedded-Systeme.

### Lernziele

Nach dem Durcharbeiten dieses Dokuments kannst du:

1. **SSOT anwenden** – Konfigurationen an genau einer Stelle definieren und referenzieren
2. **Schichten trennen** – Abhängigkeiten gerichtet halten, Logik von Hardware isolieren
3. **Pipelines konstruieren** – Datenflüsse sequentiell und vorhersagbar gestalten
4. **Invarianten durchsetzen** – Systemweite Garantien (z. B. One-Hot) implementieren
5. **Edge-basiert denken** – Ereignisse statt Pegel für UI-Aktionen verwenden
6. **Testbar entwerfen** – Logik isoliert prüfen, Hardware end-to-end validieren

### Begriffsdefinitionen

| Begriff        | Definition                                                                  | Quelle/Norm        |
|----------------|-----------------------------------------------------------------------------|--------------------|
| **SSOT**       | Single Source of Truth – eine Konfiguration existiert an genau einer Stelle | Design-Patterns    |
| **Invariante** | Bedingung, die zu jedem Zeitpunkt gelten muss (vgl. Hoare-Logik)            | Dijkstra (1976)    |
| **One-Hot**    | Codierung: genau ein Bit gesetzt (oder keins), niemals mehrere              | Digital-Design     |
| **Pipeline**   | Sequenz von Verarbeitungsstufen mit definierten Ein-/Ausgängen              | Hennessy/Patterson |
| **Edge**       | Zustandsübergang (Flanke), nicht der Zustand selbst                         | IEC 61131-3        |
| **HAL**        | Hardware Abstraction Layer – trennt Hardware-Zugriff von Anwendungslogik    | AUTOSAR, CMSIS     |
| **Debounce**   | Entprellung – Filterung mechanischer Prellsignale durch Zeit-Schwellwert    | Embedded-Praxis    |

---

## 1) SSOT – Hardware-Konfiguration ist kein „Detail"

### Problemstellung

Wenn Pins, Zählerstände oder SPI-Parameter an mehreren Stellen definiert sind, entstehen Inkonsistenzen. Du änderst einen Pin in `main.cpp`, vergisst aber den Treiber – das System verhält sich unerklärlich.

> **Merkregel:** Eine Zahl, eine Stelle. Wer kopiert, verliert.

### Herleitung

Wir betrachten ein Beispiel: `BTN_COUNT` erscheint in drei Dateien – `main.cpp`, `cd4021.cpp`, `debounce.cpp`. Bei einer Änderung von 10 auf 100 Kanäle vergisst du eine Stelle. Das Ergebnis: Der Treiber liest 13 Bytes (100 Buttons), aber die Logik erwartet 2 Bytes (10 Buttons). Das Array überläuft – ein klassischer Buffer-Overflow.

### Regel

Jede Konfiguration existiert an genau einer Stelle. Alle anderen Dateien referenzieren diese Quelle.

### Anwendung

Die SSOT-Dateien liegen im Header-Verzeichnis:

```
firmware/include/hw/
├── board_pins.h    # Pin-Definitionen (GPIO-Nummern)
├── io_counts.h     # Kanalzähler und Masken
└── spi_params.h    # SPI-Frequenz, Mode, Bit-Order
```

**Beispiel aus `io_counts.h`:**

```c
#define BTN_COUNT       10
#define LED_COUNT       10

#define BTN_BYTES       ((BTN_COUNT + 7) / 8)  // = 2 (bei 10)
#define LED_BYTES       ((LED_COUNT + 7) / 8)  // = 2 (bei 10)

// Maske für gültige Bits im letzten Byte
// Bei 10 LEDs: 16 Bits verfügbar, nur 10 genutzt
// Byte[1]: Bits 7,6 gültig → Maske = 0xC0
#define LED_LAST_MASK   0xC0
```

**Zahlenbeispiel zur Byte-Berechnung:**

| BTN_COUNT | Berechnung              | BTN_BYTES |
|-----------|-------------------------|-----------|
| 8         | (8 + 7) / 8 = 15 / 8    | 1         |
| 10        | (10 + 7) / 8 = 17 / 8   | 2         |
| 16        | (16 + 7) / 8 = 23 / 8   | 2         |
| 100       | (100 + 7) / 8 = 107 / 8 | 13        |

Die Division ist ganzzahlig (Integer-Arithmetik). Der Trick `+7` rundet auf – ohne ihn würde `BTN_COUNT=9` nur 1 Byte ergeben, obwohl 9 Bits nicht in 8 Bits passen.

> **Merkregel:** `(n + 7) / 8` = Anzahl Bytes für n Bits (Aufrunden durch Integer-Division).

### Zusammenfassung

Wenn du von 10 auf 100 Kanäle skalierst, änderst du `BTN_COUNT` und `LED_COUNT` an dieser einen Stelle. Die abgeleiteten Makros `BTN_BYTES` und `LED_LAST_MASK` berechnen sich automatisch – keine weiteren Code-Anpassungen nötig.

---

## 2) Schichtenmodell – Abhängigkeiten zeigen nach unten

### Problemstellung

Ohne klare Schichtung entsteht „Spaghetti-Code": Der Treiber ruft Logik auf, die Logik greift auf Hardware zu, die Anwendung dupliziert Regeln. Änderungen haben unvorhersehbare Auswirkungen.

### Regel

Abhängigkeiten sind gerichtet. Höhere Schichten dürfen niedrigere nutzen, aber nicht umgekehrt.

### Herleitung

Wir zeichnen das Schichtenmodell als Stapel. Jeder Pfeil zeigt nach unten (erlaubte Abhängigkeit) oder ist verboten (nach oben):

```
┌─────────────────────────────────────────────────────────────┐
│  app/         Orchestrierung (Loop/Task, Scheduling)        │  ← darf alles darunter nutzen
├─────────────────────────────────────────────────────────────┤
│  logic/       Regeln (Debounce, Selection, Pipeline)        │  ← darf drivers/ und darunter nutzen
├─────────────────────────────────────────────────────────────┤
│  drivers/     IC-spezifisch (CD4021 read, 74HC595 write)    │  ← darf hal/ und hw/ nutzen
├─────────────────────────────────────────────────────────────┤
│  hal/         SPI-Bus-Zugriff (Transfer, Settings)          │  ← darf nur hw/ nutzen
├─────────────────────────────────────────────────────────────┤
│  include/hw/  SSOT (Pins, Counts, SPI-Params)               │  ← Basis, keine Abhängigkeiten
└─────────────────────────────────────────────────────────────┘
```

**Verzeichnisstruktur (konkret):**

```
firmware/src/
├── hal/
│   └── spi_bus.cpp         # SPI-Init, Transfer-Funktion
├── drivers/
│   ├── cd4021.cpp          # Button-Input (liest Bytes)
│   └── hc595.cpp           # LED-Output (schreibt Bytes)
├── logic/
│   ├── debounce.cpp        # Entprellung → Press-Edges
│   ├── selection.cpp       # Selection-Policy → selectedId
│   └── io_pipeline.cpp     # Orchestriert Logik-Module
└── app/
    ├── io_loop.cpp         # Arduino-Variante (loop())
    └── io_task.cpp         # FreeRTOS-Variante (Task)
```

### Anwendung: Was darf welche Schicht?

| Schicht    | Darf                                   | Darf nicht                            |
|------------|----------------------------------------|---------------------------------------|
| `app/`     | Schichten orchestrieren, Tasks starten | Regeln duplizieren, direkt SPI nutzen |
| `logic/`   | IDs, Zustände, Edges verarbeiten       | Pins kennen, SPI aufrufen             |
| `drivers/` | Bytes lesen/schreiben über HAL         | Debounce-Entscheidungen treffen       |
| `hal/`     | SPI-Register konfigurieren             | Button-Logik, LED-Zustände kennen     |

### Zusammenfassung

Die `logic/`-Schicht ist hardware-unabhängig. Du kannst sie auf dem Host-PC unit-testen – ohne ESP32, ohne SPI, ohne echte Hardware. Das ist der Kern der Testbarkeit (siehe Abschnitt 8).

> **Merkregel:** „Logik kennt keine Pins." Wenn deine Logik `#include <Arduino.h>` braucht, ist die Trennung gebrochen.

---

## 3) Datenfluss ist eine Pipeline

### Problemstellung

Wenn IO-Code „verstreut" ist – hier ein LED-Update, dort ein Button-Check – wird das System unvorhersehbar. Die One-Hot-Invariante (Abschnitt 4) kann brechen, weil zwei Codepfade unabhängig LEDs setzen.

### Regel

Der gesamte IO-Zyklus folgt einer klaren Sequenz:

```
read(raw) → logic(process) → derive(outputs) → write(outputs)
```

### Herleitung

Wir betrachten den konkreten Datenfluss im Selection Panel:

```
CD4021::read(rawButtons)        ← Hardware-Input
         │
         ▼
┌────────────────────────────────────────────────────┐
│                    io_pipeline                     │
│  ┌──────────┐   ┌────────────┐   ┌──────────────┐  │
│  │ Debounce │ → │Press-Guard │ → │  Selection   │  │
│  │ (Edges)  │   │ (optional) │   │  (Policy)    │  │
│  └──────────┘   └────────────┘   └──────────────┘  │
└────────────────────────────────────────────────────┘
         │
         ▼ selectedId (uint8_t, 0..N)
    setOneHot(selectedId) → ledState[]
         │
         ▼
HC595::write(ledState)          ← Hardware-Output
```

**Was passiert in jeder Stufe?**

| Stufe          | Eingabe             | Ausgabe               | Funktion                               |
|----------------|---------------------|-----------------------|----------------------------------------|
| CD4021::read() | –                   | rawButtons[BTN_BYTES] | Liest Schieberegister (active-low)     |
| Debounce       | rawButtons[]        | pressEdges (Bitfeld)  | Filtert Prellen, erzeugt Flanken       |
| Press-Guard    | pressEdges          | validEdge (bool)      | Verhindert Doppel-Press (optional)     |
| Selection      | validEdge, buttonId | selectedId            | Wendet Policy an (Toggle, Radio, etc.) |
| setOneHot()    | selectedId          | ledState[LED_BYTES]   | Wandelt ID in Byte-Array               |
| HC595::write() | ledState[]          | –                     | Schiebt Bytes zum Schieberegister      |

### Anwendung

**Kernregel:** LED-Zustände entstehen an genau einer Stelle (`setOneHot`). Kein anderer Code darf LEDs direkt setzen – das würde die Pipeline umgehen und könnte One-Hot brechen.

```c
// ✗ Schlecht: LED direkt setzen (umgeht Pipeline)
ledState[0] |= 0x80;  // Setzt LED 1 „nebenbei"

// ✓ Gut: Immer über Pipeline
uint8_t newSelection = computeSelection(currentSelection, pressedButton);
setOneHot(newSelection, ledState, LED_BYTES);
```

### Zusammenfassung

> **Merkregel:** „Ein Eingang, ein Ausgang, eine Richtung." Die Pipeline ist ein Fließband – Teile überspringen erzeugt Chaos.

---

## 4) One-Hot ist eine systemweite Invariante

### Problemstellung

Bei einem Panel mit 100 LEDs wäre es fatal, wenn versehentlich zwei LEDs gleichzeitig leuchten oder ein ungültiger Index adressiert wird. Das Ergebnis: Verwirrung für den Nutzer, möglicherweise Überstrom durch die Treiber-ICs.

### Regel

One-Hot ist ein Vertrag, der zu jedem Zeitpunkt gilt:

- `selectedId == 0` → alle LEDs aus
- `selectedId ∈ [1..N]` → genau eine LED an (LED Nummer `selectedId`)
- Andere Zustände sind verboten (undefiniertes Verhalten)

### Herleitung

Wir implementieren die Invariante zentral in einer Funktion:

```c
void setOneHot(uint8_t selectedId, uint8_t* ledState, size_t ledBytes) {
    // Schritt 1: Alle LEDs aus (clear)
    memset(ledState, 0x00, ledBytes);

    // Schritt 2: Genau eine LED setzen (wenn selectedId > 0)
    if (selectedId >= 1 && selectedId <= LED_COUNT) {
        uint8_t byteIdx = (selectedId - 1) / 8;
        uint8_t bitIdx  = 7 - ((selectedId - 1) % 8);  // MSB-first
        ledState[byteIdx] |= (1 << bitIdx);
    }

    // Schritt 3: Ungültige Bits im letzten Byte maskieren
    ledState[ledBytes - 1] &= LED_LAST_MASK;
}
```

**Warum Schritt 3?** Bei 10 LEDs brauchst du 2 Bytes (16 Bits), aber nur 10 sind gültig. Die Bits 5–0 im zweiten Byte könnten durch Bugs oder Speicherfehler gesetzt sein. Die Maske garantiert, dass sie niemals an den 74HC595 gelangen.

### Anwendung: Zahlenbeispiel

Für 10 LEDs, MSB-first-Anordnung:

| selectedId | Byte[0]    | Byte[1]    | Hex         | Bedeutung     |
|------------|------------|------------|-------------|---------------|
| 0 (aus)    | `00000000` | `00000000` | `0x00 0x00` | Alle LEDs aus |
| 1          | `10000000` | `00000000` | `0x80 0x00` | LED 1 an      |
| 8          | `00000001` | `00000000` | `0x01 0x00` | LED 8 an      |
| 9          | `00000000` | `10000000` | `0x00 0x80` | LED 9 an      |
| 10         | `00000000` | `01000000` | `0x00 0x40` | LED 10 an     |

**Bit-Position-Berechnung (Schritt 2):**

```
selectedId = 9
byteIdx = (9 - 1) / 8 = 8 / 8 = 1  → Byte[1]
bitIdx  = 7 - ((9 - 1) % 8) = 7 - 0 = 7  → Bit 7 (MSB)
Ergebnis: Byte[1] = 0b10000000 = 0x80
```

> **Merkregel:** „One-Hot = Clear-then-Set." Erst alles löschen, dann genau eines setzen – niemals umgekehrt.

### Zusammenfassung

Die Funktion `setOneHot()` ist die einzige Stelle, die LED-Zustände erzeugt. Sie garantiert die Invariante durch Konstruktion: Es ist unmöglich, zwei LEDs gleichzeitig zu setzen, solange alle LED-Updates durch diese Funktion laufen.

---

## 5) Ereignisse statt Pegel – Buttons sind Edge-getrieben

### Problemstellung

Wenn die Logik auf „Button ist gedrückt" (Pegel) reagiert, feuert sie bei jedem Scan-Zyklus. Bei 10 ms Scan-Intervall wären das 100 Events pro Sekunde, solange du die Taste hältst – ein „Event-Spam", der die Selection-Logik überflutet.

### Regel

UI-Aktionen basieren auf Press-Edges (Flanken), nicht auf Pegeln.

```
CD4021 (Pegel)      Debounce            Selection
     │                  │                   │
  active-low    →   pressed edges    →   reagiert nur
  (0=gedrückt)     (stabil+Flanke)       auf Edges
```

### Herleitung

*Analogie:* Ein Lichtschalter reagiert auf das Umlegen (Flanke), nicht auf die Position (Pegel). Würde das Licht bei jedem „Schalter ist oben"-Check ein-/ausschalten, flackerte es im Takt der Abfrage. Die Analogie endet dort, wo es um Timing geht: Mechanische Schalter prellen (10–50 ms), digitale Signale nicht.

**Debounce-Algorithmus (vereinfacht):**

```c
typedef struct {
    uint8_t stable;      // Letzter stabiler Zustand (0 oder 1)
    uint8_t counter;     // Zähler für stabile Abtastwerte
} DebounceState;

bool debounce_update(DebounceState* state, uint8_t raw, uint8_t threshold) {
    if (raw == state->stable) {
        state->counter = 0;   // Zustand unverändert → Reset
        return false;
    }

    state->counter++;
    if (state->counter >= threshold) {
        bool wasPressed = (state->stable != 0);
        bool isPressed  = (raw != 0);
        state->stable = raw;
        state->counter = 0;

        // Press-Edge: war nicht gedrückt, ist jetzt gedrückt
        return (!wasPressed && isPressed);
    }
    return false;  // Noch nicht stabil genug
}
```

**Timing-Berechnung:**

| Parameter         | Wert     | Berechnung              |
|-------------------|----------|-------------------------|
| Scan-Intervall    | 10 ms    | Vorgabe (Task-Periode)  |
| Threshold         | 3        | Anzahl stabiler Samples |
| Entprellzeit      | 30 ms    | 3 × 10 ms               |
| Typisches Prellen | 10–50 ms | Mechanische Taster      |

Mit `threshold = 3` bei 10 ms Scan-Intervall ergibt sich eine Entprellzeit von 30 ms – ausreichend für typische mechanische Taster.

### Anwendung

Die Selection-Logik sieht nur „Button 7 wurde gerade gedrückt" (ein einziges Event), nicht „Button 7 ist seit 500 ms gedrückt" (50 Events). Das verhindert Event-Spam und macht das Verhalten vorhersagbar.

### Zusammenfassung

> **Merkregel:** „Flanke statt Pegel – ein Button, ein Event."

---

## 6) Determinismus – Timing bleibt stabil

### Problemstellung

Im Embedded-Kontext kann ein einzelner `Serial.printf()` im Hot-Path das gesamte Timing zerstören. UART bei 115200 Baud braucht etwa 87 µs pro Zeichen. Bei 50 Zeichen sind das 4,35 ms – fast die Hälfte eines 10-ms-Scan-Intervalls.

**Berechnung:**

```
Baudrate: 115200 Bit/s
Bits pro Zeichen: 10 (8 Daten + 1 Start + 1 Stop)
Zeit pro Zeichen: 10 / 115200 = 86,8 µs ≈ 87 µs
50 Zeichen: 50 × 87 µs = 4,35 ms
```

### Regel

Der Scan-Pfad läuft ohne Blockierungen. Logging erfolgt nur bei Zustandsänderungen.

| Erlaubt                        | Verboten                        |
|--------------------------------|---------------------------------|
| Logging bei Selection-Change   | Logging pro Scan-Zyklus         |
| `vTaskDelayUntil()` (FreeRTOS) | Busy-loops mit `delay()`        |
| Kurze SPI-Transfers (~10 µs)   | Lange UART-Ausgaben im Hot-Path |

### Herleitung: Timing-Budget

Wir berechnen das Timing für 10 Kanäle:

| Operation                         | Typische Dauer | Quelle             |
|-----------------------------------|----------------|--------------------|
| CD4021 read (2 Bytes @ 1 MHz SPI) | ~20 µs         | Datenblatt CD4021B |
| Debounce + Selection              | ~5 µs          | Messung            |
| HC595 write (2 Bytes @ 1 MHz SPI) | ~20 µs         | Datenblatt 74HC595 |
| **Gesamt pro Zyklus**             | ~50 µs         |                    |
| **Verfügbar bei 10 ms Intervall** | 10.000 µs      |                    |
| **Reserve**                       | 9.950 µs       | 99,5 %             |

### Anwendung: Skalierung auf 100 Kanäle

| Parameter          | 10 Kanäle | 100 Kanäle |
|--------------------|-----------|------------|
| Bytes pro Transfer | 2         | 13         |
| SPI-Transferzeit   | ~20 µs    | ~130 µs    |
| Gesamt pro Zyklus  | ~50 µs    | ~270 µs    |
| Reserve bei 10 ms  | 99,5 %    | 97,3 %     |

Bei 100 Kanälen bleibt das Timing stabil, solange du keine blockierenden Operationen einbaust.

### Zusammenfassung

> **Merkregel:** „Im Hot-Path kein printf()." Debugging gehört in Zustandsänderungen, nicht in jeden Scan.

---

## 7) Skalierbarkeit – Byte-orientiert und maskenbasiert

### Problemstellung

Code mit hartkodierten Werten wie `uint8_t buttons[2]` oder Schleifen `for (int i = 0; i < 10; i++)` muss bei Skalierung manuell angepasst werden. Jede Anpassung ist eine Fehlerquelle.

### Regel

Algorithmen arbeiten mit `*_COUNT` und `*_BYTES` Makros, nicht mit Literalen.

```c
// ✗ Schlecht: hartkodiert
uint8_t ledState[2];
for (int i = 0; i < 10; i++) { /* ... */ }

// ✓ Gut: skalierbar
uint8_t ledState[LED_BYTES];
for (int i = 0; i < LED_COUNT; i++) { /* ... */ }
```

### Herleitung: Automatische Masken-Berechnung

Die Maske für das letzte Byte muss die ungenutzten Bits ausblenden:

```c
#define LED_COUNT       100
#define LED_BYTES       ((LED_COUNT + 7) / 8)  // = 13

// Berechnung der Maske:
// 100 % 8 = 4 → 4 LEDs im letzten Byte gültig
// Bits 7, 6, 5, 4 → Maske = 0xF0
#define LED_LAST_MASK   (0xFF << (8 - (LED_COUNT % 8))) & 0xFF
```

**Zahlenbeispiele:**

| LED_COUNT | LED_COUNT % 8 | Gültige Bits   | Maske Binär | Maske Hex |
|-----------|---------------|----------------|-------------|-----------|
| 8         | 0             | alle 8         | `11111111`  | `0xFF`    |
| 10        | 2             | Bit 7, 6       | `11000000`  | `0xC0`    |
| 12        | 4             | Bit 7, 6, 5, 4 | `11110000`  | `0xF0`    |
| 100       | 4             | Bit 7, 6, 5, 4 | `11110000`  | `0xF0`    |

**Sonderfall:** Wenn `LED_COUNT % 8 == 0`, ergibt die Formel `0xFF << 8 = 0x00` – falsch! Korrigierte Formel:

```c
#define LED_LAST_MASK   ((LED_COUNT % 8) == 0 ? 0xFF : \
                         (0xFF << (8 - (LED_COUNT % 8))) & 0xFF)
```

### Anwendung

Die Änderung `BTN_COUNT=10` → `BTN_COUNT=100` erfordert keine Anpassung in der Logik. Arrays, Schleifen und Masken passen sich automatisch an.

### Zusammenfassung

> **Merkregel:** „Keine Magic Numbers. `*_COUNT` und `*_BYTES` sind deine Freunde."

---

## 8) Testbarkeit ist ein Architekturziel

### Problemstellung

Wenn Logik und Hardware verwoben sind, kannst du nur am echten Gerät testen. Jeder Test braucht Flashen, Verkabeln, manuelles Drücken – langsam und fehleranfällig.

### Regel

Logik muss isoliert testbar sein (Unit-Tests), Hardware-Pfad end-to-end (E2E-Tests).

### Herleitung: Test-Environments

In `platformio.ini` definierst du getrennte Umgebungen:

| Environment               | Typ  | Was wird getestet                                  |
|---------------------------|------|----------------------------------------------------|
| `seeed_xiao_esp32s3_test` | Unit | Nur `logic/` (Debounce, Selection, Pipeline)       |
| `seeed_xiao_esp32s3_e2e`  | E2E  | `logic/` + `drivers/` + `hal/` mit echter Hardware |

### Anwendung: Unit-Test-Beispiel

```c
#include <unity.h>

void test_debounce_filters_noise() {
    DebounceState state = {0};

    // Simuliere Prellen: 0 → 1 → 0 → 1 → 1 → 1
    TEST_ASSERT_FALSE(debounce_update(&state, 1, 3));  // Zähler = 1
    TEST_ASSERT_FALSE(debounce_update(&state, 0, 3));  // Reset auf 0
    TEST_ASSERT_FALSE(debounce_update(&state, 1, 3));  // Zähler = 1
    TEST_ASSERT_FALSE(debounce_update(&state, 1, 3));  // Zähler = 2
    TEST_ASSERT_TRUE(debounce_update(&state, 1, 3));   // Zähler = 3 → Edge!
}

void test_oneHot_sets_single_led() {
    uint8_t ledState[2] = {0xFF, 0xFF};  // Absichtlich „verschmutzt"

    setOneHot(1, ledState, 2);
    TEST_ASSERT_EQUAL_HEX8(0x80, ledState[0]);  // LED 1 an
    TEST_ASSERT_EQUAL_HEX8(0x00, ledState[1]);  // Rest aus

    setOneHot(10, ledState, 2);
    TEST_ASSERT_EQUAL_HEX8(0x00, ledState[0]);
    TEST_ASSERT_EQUAL_HEX8(0x40, ledState[1]);  // LED 10 an
}
```

**Testbefehle:**

```bash
# Unit-Tests (Logik, läuft in Sekunden)
pio test -e seeed_xiao_esp32s3_test -v

# E2E-Hardware-Tests (braucht echte Hardware)
pio test -e seeed_xiao_esp32s3_e2e -f test_e2e_hw -v
```
