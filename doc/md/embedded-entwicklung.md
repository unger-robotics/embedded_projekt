# Embedded-Entwicklung: Engineering-Regeln


## 0) Begriffsrahmen und Zielsetzung

### Anwendungskontext

Embedded-Entwicklung unterscheidet sich von Desktop-Software durch drei fundamentale Eigenschaften: begrenzte Ressourcen, harte Echtzeitanforderungen und physische Schnittstellen. Die folgenden Regeln sind keine abstrakten Best Practices, sondern konkrete Antworten auf konkrete Probleme, die bei der Arbeit mit ESP32-S3, Schieberegistern (CD4021B, 74HC595) und FreeRTOS auftreten.

### Lernziele

Nach dem Durcharbeiten dieses Dokuments kannst du:

1. **SSOT konsequent anwenden** – Hardware-Konfiguration an genau einer Stelle definieren
2. **Determinismus garantieren** – Timing-Budgets berechnen und einhalten
3. **Edge-basierte Logik implementieren** – Flanken statt Pegel auswerten
4. **One-Hot-Invarianten sicherstellen** – durch Konstruktion, nicht durch Hoffnung
5. **Skalierbare Formeln verwenden** – von 10 auf 100 Kanäle ohne Code-Änderung
6. **Schichtenarchitektur einhalten** – testbare, wartbare Strukturen bauen
7. **Teststrategie anwenden** – Unit-Tests für Logik, E2E-Tests für Hardware

### Begriffsdefinitionen

| Begriff           | Definition                                                      | Quelle/Kontext        |
|-------------------|-----------------------------------------------------------------|-----------------------|
| **Determinismus** | Gleiches Eingangssignal führt immer zum gleichen Verhalten      | Echtzeit-Systeme      |
| **SSOT**          | Single Source of Truth – jede Information hat genau eine Quelle | Design-Patterns       |
| **Hot-Path**      | Code, der in jedem Scan-Zyklus ausgeführt wird (zeitkritisch)   | Performance-Analyse   |
| **Edge**          | Zustandsübergang (Flanke), nicht der Zustand selbst             | Digital-Design        |
| **Invariante**    | Bedingung, die zu jedem Zeitpunkt gelten muss                   | Formale Verifikation  |
| **Scan-Zyklus**   | Ein vollständiger Durchlauf: Lesen → Verarbeiten → Schreiben    | Embedded-Terminologie |
| **One-Hot**       | Codierung: höchstens ein Bit gesetzt, alle anderen 0            | Digital-Design        |

### Ziel

Code, der deterministisch läuft, isoliert testbar ist und von 10 auf 100 Kanäle skaliert, ohne dass du die Logik ändern musst.

---

## 1) Hardware ist die API

### Problemstellung

In Desktop-Software definierst du Interfaces mit Methoden und Typen. Der Compiler prüft Kompatibilität, Unit-Tests verifizieren Verhalten. In Embedded-Systemen ist die „API" physisch: Pins, Spannungspegel, Timing-Constraints. Diese Schnittstelle ist nicht verhandelbar – ein falsches Signal zerstört Hardware oder produziert unerklärliches Verhalten.

### Regel

Behandle Hardware-Konfiguration mit derselben Sorgfalt wie Software-Interfaces: dokumentiert, versioniert, an einer Stelle definiert.

### Herleitung: SSOT für Hardware

| Aspekt        | SSOT-Datei                         | Beispielinhalt               |
|---------------|------------------------------------|------------------------------|
| Pin-Zuordnung | `firmware/include/hw/board_pins.h` | `#define PIN_SCK D8`         |
| Kanalanzahl   | `firmware/include/hw/io_counts.h`  | `#define LED_COUNT 10`       |
| SPI-Parameter | `firmware/include/hw/spi_params.h` | `#define CD4021_FREQ 500000` |

**Warum SSOT?**

Wenn Pin-Definitionen in drei Dateien stehen (main.cpp, driver.cpp, test.cpp) und du eine änderst, sind die anderen falsch. Das System verhält sich unerklärlich, und die Fehlersuche dauert Stunden.

### Anwendung: Code-Muster

**Verboten – Pin hardkodiert:**

```c
// ✗ SCHLECHT: Wo kommt D1 her? Was ist D1? Welche anderen Stellen nutzen D1?
void cd4021_init() {
    pinMode(D1, OUTPUT);
}
```

**Richtig – Pin aus SSOT:**

```c
// ✓ GUT: Selbstdokumentierend, zentral definiert
#include "hw/board_pins.h"

void cd4021_init() {
    pinMode(PIN_BTN_PS, OUTPUT);  // PS = Parallel/Serial Load
}
```

### Herleitung: Compiler erzwingt Konsistenz

```
┌─────────────────────────────────────────────────────────────────────┐
│                     board_pins.h (SSOT)                             │
│  #define PIN_SCK    D8                                              │
│  #define PIN_MISO   D9                                              │
│  #define PIN_BTN_PS D1                                              │
│  #define PIN_LATCH  D2                                              │
└───────────────────────────┬─────────────────────────────────────────┘
                            │
          ┌─────────────────┼─────────────────┐
          │                 │                 │
          ▼                 ▼                 ▼
    ┌──────────┐      ┌──────────┐      ┌──────────┐
    │ hal/spi  │      │ drivers/ │      │  test/   │
    │ #include │      │ #include │      │ #include │
    │ board_   │      │ board_   │      │ board_   │
    │ pins.h   │      │ pins.h   │      │ pins.h   │
    └──────────┘      └──────────┘      └──────────┘
```

Alle Dateien referenzieren dieselbe Quelle. Änderst du `board_pins.h`, kompilieren alle neu und nutzen den neuen Wert.

### Zusammenfassung

> **Merkregel:** „Eine Pin-Nummer, eine Datei. Der Compiler ist dein Konsistenz-Wächter."

---

## 2) Determinismus vor Flexibilität

### Problemstellung

Embedded-Systeme müssen in definierten Zeitfenstern reagieren. Ein Scan-Zyklus von 10 ms bedeutet: Nach exakt 10 ms muss die nächste Messung starten – nicht „ungefähr 10 ms", nicht „wenn gerade Zeit ist". Verpasste Deadlines führen zu träger Reaktion, verpassten Events oder Kommunikationsabbrüchen.

### Regel

Der Worst-Case zählt, nicht der Durchschnitt. Wenn eine Operation manchmal 5 ms und manchmal 50 ms dauert, ist sie im 10-ms-Zyklus unbrauchbar.

### Herleitung: Scan-Zyklus-Struktur

```
┌──────────────────────────────────────────────────────────────────────┐
│  Scan-Zyklus (10 ms Budget)                                          │
│                                                                      │
│  read(raw)  →  update(state)  →  derive(output)  →  write(leds)     │
│      │              │                 │                │             │
│   CD4021        Debounce          setOneHot()       HC595           │
│   ~20 µs        Selection          ~1 µs            ~20 µs          │
│                  ~5 µs                                               │
└──────────────────────────────────────────────────────────────────────┘
```

### Anwendung: Timing-Budget berechnen

**10 Kanäle:**

| Phase       | Dauer    | Anteil     | Anmerkung              |
|-------------|----------|------------|------------------------|
| read()      | ~20 µs   | 0,20%      | 2 Bytes SPI @ 500 kHz  |
| update()    | ~5 µs    | 0,05%      | Debounce + Selection   |
| derive()    | ~1 µs    | 0,01%      | One-Hot-Konvertierung  |
| write()     | ~20 µs   | 0,20%      | 2 Bytes SPI @ 1 MHz    |
| **Summe**   | ~46 µs   | **0,46%**  |                        |
| **Reserve** | ~9954 µs | **99,54%** | Für Kommunikation etc. |

**100 Kanäle (Skalierung):**

| Phase       | Dauer    | Anteil     | Anmerkung              |
|-------------|----------|------------|------------------------|
| read()      | ~130 µs  | 1,30%      | 13 Bytes SPI @ 500 kHz |
| update()    | ~50 µs   | 0,50%      | 100× Debounce          |
| derive()    | ~5 µs    | 0,05%      | One-Hot (13 Bytes)     |
| write()     | ~65 µs   | 0,65%      | 13 Bytes SPI @ 1 MHz   |
| **Summe**   | ~250 µs  | **2,50%**  |                        |
| **Reserve** | ~9750 µs | **97,50%** | Noch komfortabel       |

### Herleitung: Was zerstört Determinismus?

| Gut ✓                     | Schlecht ✗            | Warum schlecht               |
|---------------------------|-----------------------|------------------------------|
| `vTaskDelayUntil()`       | `vTaskDelay()`        | Drift akkumuliert            |
| Logging bei Events        | Logging pro Scan      | UART blockiert (4+ ms/Zeile) |
| Feste Zykluszeit          | `delay()` im Hot-Path | Timing wird unvorhersagbar   |
| Interrupt-basierte Events | Busy-Wait-Polling     | CPU-Zeit verschwendet        |

### Anwendung: FreeRTOS-Timing

**Falsch – Drift akkumuliert:**

```c
void io_task(void* param) {
    while (true) {
        scan_cycle();                        // Dauert ~46 µs
        vTaskDelay(pdMS_TO_TICKS(10));       // Wartet 10 ms NACH Ende
    }
    // Tatsächliche Periode: 10 ms + 46 µs + Scheduling-Overhead
    // Nach 1000 Zyklen: ~46 ms Drift
}
```

**Richtig – Feste Periode:**

```c
void io_task(void* param) {
    TickType_t lastWake = xTaskGetTickCount();
    while (true) {
        scan_cycle();                                    // Dauert ~46 µs
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(10));   // Wartet bis 10 ms AB Start
    }
    // Tatsächliche Periode: exakt 10 ms (unabhängig von Zyklusdauer)
}
```

**Timing-Visualisierung:**

```
vTaskDelay():
├─ Scan ─┤     10 ms Delay      ├─ Scan ─┤     10 ms Delay      ├─...
0      46µs                    10,05ms  10,1ms                  20,1ms
         └──────── Drift ────────┘

vTaskDelayUntil():
├─ Scan ─┤     Delay (Rest)     ├─ Scan ─┤     Delay (Rest)     ├─...
0      46µs                    10ms     10,05ms                 20ms
         └──────── Exakt ────────┘
```

### Zusammenfassung

> **Merkregel:** „Im Hot-Path keine Überraschungen. Messe Worst-Case, rechne vorher, `vTaskDelayUntil()` für feste Perioden."

---

## 3) Edges statt Pegel

### Problemstellung

Wenn du auf den Pegel „Button ist gedrückt" reagierst, feuert die Logik in jedem Scan-Zyklus. Bei 10 ms Intervall sind das 100 Events pro Sekunde, solange der Benutzer die Taste hält – eine Sekunde Drücken erzeugt 100 Selection-Wechsel.

### Regel

UI-Aktionen reagieren auf Flanken (Edges), nicht auf Pegel. Ein Button-Druck ist ein einmaliges Event, kein kontinuierlicher Zustand.

### Herleitung: Pegel vs. Edge

**Pegel-basiert (problematisch):**

```
Button-Zustand:    ████████████████████████████  (500 ms gehalten)
Scan-Intervall:   │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ (alle 10 ms)
Events:           ↑ ↑ ↑ ↑ ↑ ↑ ↑ ↑ ↑ ↑ ↑ ↑ ↑ ↑ ↑ (50 Events!)
```

**Edge-basiert (korrekt):**

```
Button-Zustand: ────┐████████████████████████┌────
                    │                        │
Events:             ↓                        ↑
                 Press-Edge              Release-Edge
                 (1 Event)               (ignoriert)
```

### Anwendung: Pipeline-Architektur

```
CD4021::read()  →  Debounce  →  Edge-Detect  →  Selection
      │                │             │              │
   raw[] = 0xFD    stable[] = 0xFD  pressEdge    selectedId = 3
   (Bit 1 = 0)     (nach 50 ms)    = {true, 1}   (einmalig)
   Pegel           Stabiler Pegel  Flanke        Aktion
```

| Stufe       | Eingabe   | Ausgabe          | Verantwortung   |
|-------------|-----------|------------------|-----------------|
| read()      | Hardware  | raw[] (Pegel)    | SPI-Transfer    |
| Debounce    | raw[]     | stable[] (Pegel) | Prellen filtern |
| Edge-Detect | stable[]  | pressEdge (bool) | Flanke erkennen |
| Selection   | pressEdge | selectedId       | Geschäftslogik  |

### Anwendung: Code-Implementierung

```c
typedef struct {
    uint8_t stable;      // Letzter stabiler Zustand (nach Debounce)
    uint8_t previous;    // Vorheriger Zustand für Edge-Detection
    uint8_t counter;     // Debounce-Zähler
} ButtonState;

/**
 * Erkennt Press-Edge nach Debouncing.
 *
 * @param state    Button-Zustand (wird aktualisiert)
 * @param current  Aktueller Rohwert vom CD4021
 * @param mask     Bit-Maske für diesen Button
 * @param threshold Debounce-Schwelle (Anzahl stabiler Samples)
 * @return true wenn Press-Edge erkannt
 */
bool detectPressEdge(ButtonState* state, uint8_t current,
                     uint8_t mask, uint8_t threshold) {
    // 1. Debounce: Nur stabile Zustände akzeptieren
    uint8_t currentBit = (current & mask) ? 1 : 0;
    uint8_t stableBit  = (state->stable & mask) ? 1 : 0;

    if (currentBit == stableBit) {
        state->counter = 0;  // Zustand bestätigt
    } else {
        state->counter++;
        if (state->counter >= threshold) {
            // Neuer stabiler Zustand
            if (currentBit) {
                state->stable |= mask;
            } else {
                state->stable &= ~mask;
            }
            state->counter = 0;
        } else {
            return false;  // Noch nicht stabil
        }
    }

    // 2. Edge-Detection: War nicht gedrückt, ist jetzt gedrückt?
    bool wasPressed = (state->previous & mask) == 0;  // Active-Low
    bool isPressed  = (state->stable & mask) == 0;

    state->previous = state->stable;

    // Press-Edge: Übergang von nicht-gedrückt zu gedrückt
    return (!wasPressed && isPressed);
}
```

### Herleitung: Warum ist Edge-basierte Logik besser?

| Aspekt          | Pegel-basiert           | Edge-basiert              |
|-----------------|-------------------------|---------------------------|
| Events/Sekunde  | 100 (bei 10 ms Scan)    | 1 (pro Drücken)           |
| Testbarkeit     | Zustandsabhängig        | Deterministisch           |
| Verhalten       | „Button 3 ist gedrückt" | „Button 3 wurde gedrückt" |
| Selection-Logik | Toggelt 100×            | Toggelt 1×                |

### Zusammenfassung

> **Merkregel:** „Pegel ist Zustand, Edge ist Event. UI reagiert auf Events, nicht auf Zustände."

---

## 4) One-Hot ist Pflicht

### Problemstellung

Bei einem Selection Panel mit 100 LEDs wäre es fatal, wenn versehentlich zwei LEDs gleichzeitig leuchten. Das kann passieren durch:
- Vergessenes Löschen des alten Bits
- Race Condition zwischen Löschen und Setzen
- Verstreute LED-Manipulationen an verschiedenen Code-Stellen

### Regel

One-Hot-Zustände entstehen an genau einer Stelle im Code – durch Konstruktion, nicht durch Hoffnung.

### Herleitung: Die One-Hot-Invariante

```
Invariante:  popcount(ledState) ≤ 1

selectedId == 0      →  ledState = 00000000 00000000  (alle aus)
selectedId == 1      →  ledState = 10000000 00000000  (genau LED 1)
selectedId == 5      →  ledState = 00001000 00000000  (genau LED 5)
selectedId == 10     →  ledState = 00000000 01000000  (genau LED 10)

VERBOTEN:              ledState = 10001000 00000000  (2 Bits!)
```

(popcount = Anzahl gesetzter Bits)

### Anwendung: Zentrale Implementierung

```c
/**
 * Setzt LED-Zustand im One-Hot-Format.
 *
 * Garantiert: Nach Aufruf ist popcount(ledState) ≤ 1
 *
 * @param selectedId  0 = alle aus, 1..LED_COUNT = diese LED an
 * @param ledState    Output-Array (wird komplett überschrieben)
 * @param bytes       Größe des Arrays
 */
void setOneHot(uint8_t selectedId, uint8_t* ledState, size_t bytes) {
    // Schritt 1: ALLE Bits löschen (garantiert sauberen Ausgangszustand)
    memset(ledState, 0x00, bytes);

    // Schritt 2: Validierung
    if (selectedId < 1 || selectedId > LED_COUNT) {
        return;  // Alle LEDs aus (bereits durch memset)
    }

    // Schritt 3: Genau EIN Bit setzen
    uint8_t byteIdx = (selectedId - 1) / 8;
    uint8_t bitPos  = 7 - ((selectedId - 1) % 8);  // MSB-first
    ledState[byteIdx] |= (1u << bitPos);

    // Schritt 4: Ungültige Bits im letzten Byte maskieren
    ledState[bytes - 1] &= LED_LAST_MASK;
}
```

### Herleitung: Warum memset zuerst?

**Unsicherer Ansatz (zwei separate Operationen):**

```c
// ✗ GEFÄHRLICH: Race Condition möglich
void setOneHot_UNSAFE(uint8_t oldId, uint8_t newId, uint8_t* ledState) {
    // Schritt 1: Altes Bit löschen
    uint8_t oldByte = (oldId - 1) / 8;
    uint8_t oldMask = 1u << (7 - ((oldId - 1) % 8));
    ledState[oldByte] &= ~oldMask;

    // ← HIER könnte ein Interrupt die LED-Daten senden
    // ← Der Empfänger sieht: 0 LEDs an (kurzzeitig)

    // Schritt 2: Neues Bit setzen
    uint8_t newByte = (newId - 1) / 8;
    uint8_t newMask = 1u << (7 - ((newId - 1) % 8));
    ledState[newByte] |= newMask;
}
```

**Problem-Szenario:**

```
Zeit    oldId=3    newId=7    ledState         Invariante
────────────────────────────────────────────────────────
t0      aktiv      -          00100000         ✓ (1 Bit)
t1      löschen    -          00000000         ✓ (0 Bits)
t2      INTERRUPT: LED-Daten senden → Benutzer sieht: alle aus
t3      -          setzen     00000010         ✓ (1 Bit)
```

**Sicherer Ansatz (atomare Operation auf Zustand):**

```c
// ✓ SICHER: memset + ein |= ist praktisch atomar
memset(ledState, 0x00, bytes);      // Alles aus
ledState[byteIdx] |= mask;          // Genau eins an
```

Selbst wenn ein Interrupt zwischen den Zeilen kommt, ist der Zustand immer gültig (0 oder 1 Bit gesetzt).

### Anwendung: Verbotene Muster

```c
// ✗ VERBOTEN: LEDs "nebenbei" setzen
void handleSpecialCase() {
    ledState[0] |= 0x80;  // LED 1 zusätzlich an – bricht One-Hot!
}

// ✗ VERBOTEN: LEDs direkt manipulieren statt setOneHot()
void updateLeds() {
    if (someCondition) {
        ledState[0] |= 0x40;  // Wer garantiert, dass andere Bits 0 sind?
    }
}

// ✓ ERLAUBT: Immer über setOneHot()
void updateLeds() {
    if (someCondition) {
        setOneHot(2, ledState, LED_BYTES);  // Garantiert One-Hot
    }
}
```

### Zusammenfassung

> **Merkregel:** „One-Hot = Clear-then-Set. Alle LED-Änderungen gehen durch setOneHot(). Keine Ausnahmen."

---

## 5) Skalierung durch Formeln

### Problemstellung

Code mit hardkodierten Werten wie `uint8_t leds[2]` oder `for (int i = 0; i < 10; i++)` muss bei Skalierung manuell angepasst werden. Bei 100 Kanälen musst du jede „2" und jede „10" finden und ändern – fehleranfällig und zeitaufwändig.

### Regel

Alle Größen, die von der Kanalanzahl abhängen, werden aus `*_COUNT` abgeleitet. Hardkodierte Zahlen sind verboten.

### Herleitung: SSOT-Definitionen

```c
// firmware/include/hw/io_counts.h

// SSOT: Nur diese Zeilen ändern bei Skalierung
#define BTN_COUNT  10
#define LED_COUNT  10

// Abgeleitete Werte (automatisch korrekt)
#define BTN_BYTES  ((BTN_COUNT + 7) / 8)
#define LED_BYTES  ((LED_COUNT + 7) / 8)

// Berechnung: (10 + 7) / 8 = 17 / 8 = 2 (Integer-Division)
// Bei 100: (100 + 7) / 8 = 107 / 8 = 13

// Maske für gültige Bits im letzten Byte
// Bei 10 Kanälen: 10 % 8 = 2 → Bits 7, 6 gültig → Maske = 0xC0
// Bei 100 Kanälen: 100 % 8 = 4 → Bits 7, 6, 5, 4 gültig → Maske = 0xF0
#define LED_LAST_MASK  (0xFF << (8 - (LED_COUNT % 8 == 0 ? 8 : LED_COUNT % 8)))
```

### Anwendung: Richtig vs. Falsch

| Richtig ✓       | Falsch ✗ | Warum falsch                      |
|-----------------|----------|-----------------------------------|
| `LED_COUNT`     | `10`     | Bei Änderung jede Stelle suchen   |
| `LED_BYTES`     | `2`      | Inkonsistent bei Skalierung       |
| `LED_LAST_MASK` | `0xC0`   | Muss manuell neu berechnet werden |
| `i < LED_COUNT` | `i < 10` | Vergessen bei Skalierung          |

**Code-Beispiel:**

```c
// ✗ FALSCH: Hardkodiert
uint8_t ledState[2];  // Was wenn 100 Kanäle?
for (int i = 0; i < 10; i++) { ... }

// ✓ RICHTIG: Aus SSOT abgeleitet
uint8_t ledState[LED_BYTES];  // Automatisch 2 oder 13
for (int i = 0; i < LED_COUNT; i++) { ... }
```

### Herleitung: Skalierungsbeispiel

| Parameter       | 10 Kanäle | 100 Kanäle | Formel                             |
|-----------------|-----------|------------|------------------------------------|
| `LED_COUNT`     | 10        | **100**    | SSOT (manuell ändern)              |
| `LED_BYTES`     | 2         | 13         | `(LED_COUNT + 7) / 8`              |
| `LED_LAST_MASK` | 0xC0      | 0xF0       | Formel oben                        |
| Array-Größe     | 2 Bytes   | 13 Bytes   | `uint8_t ledState[LED_BYTES]`      |
| Loop-Grenze     | 10        | 100        | `i < LED_COUNT`                    |
| SPI-Transfer    | 2 Bytes   | 13 Bytes   | `spiTransfer(ledState, LED_BYTES)` |
| **Logik-Code**  | –         | –          | **Keine Änderung nötig**           |

### Anwendung: Skalierung durchführen

**Schritt 1:** Ändere `io_counts.h`:

```c
#define BTN_COUNT  100
#define LED_COUNT  100
```

**Schritt 2:** Kompiliere. Fertig.

Alle abgeleiteten Werte (`LED_BYTES`, `LED_LAST_MASK`) und alle Schleifen/Arrays, die diese Makros verwenden, sind automatisch korrekt.

### Herleitung: Bit-Masken-Berechnung

**Formel für `LED_LAST_MASK`:**

```
LED_COUNT = 10
LED_COUNT % 8 = 2 (2 gültige Bits im letzten Byte)

Gültige Bits: Bit 7, Bit 6 (MSB-first)
Binär:        11000000
Hex:          0xC0

Berechnung:
  gültigeBits = LED_COUNT % 8
  wenn gültigeBits == 0: gültigeBits = 8
  LED_LAST_MASK = 0xFF << (8 - gültigeBits)
                = 0xFF << (8 - 2)
                = 0xFF << 6
                = 0b11000000
                = 0xC0
```

**Für 100 Kanäle:**

```
LED_COUNT = 100
LED_COUNT % 8 = 4 (4 gültige Bits)

Gültige Bits: Bit 7, 6, 5, 4
Binär:        11110000
Hex:          0xF0
```

### Zusammenfassung

> **Merkregel:** „Keine magischen Zahlen. Alles aus *_COUNT ableiten. Skalierung = eine Zeile ändern."

---

## 6) Schichten einhalten

### Problemstellung

Wenn Treiber Geschäftslogik enthalten und Logik Hardware-Zugriffe macht, wird das System:
- **Untestbar:** Du kannst die Logik nicht ohne Hardware testen
- **Unwartbar:** Änderungen an einer Stelle brechen andere
- **Unportierbar:** Plattformwechsel erfordert Rewrite

### Regel

Abhängigkeiten zeigen nach unten. Höhere Schichten nutzen niedrigere, nie umgekehrt.

### Herleitung: Schichtenmodell

```
┌─────────────────────────────────────────────────────────────────────┐
│  app/            Orchestrierung                                     │
│                  Loop/Task, Timing, Initialisierung                 │
│                  Ruft auf, implementiert nichts                     │
├─────────────────────────────────────────────────────────────────────┤
│  logic/          Geschäftsregeln                                    │
│                  Debounce, Selection, Pipeline                      │
│                  KEINE Hardware-Abhängigkeiten                      │
├─────────────────────────────────────────────────────────────────────┤
│  drivers/        IC-spezifischer Zugriff                            │
│                  CD4021, 74HC595                                    │
│                  KEINE Geschäftslogik                               │
├─────────────────────────────────────────────────────────────────────┤
│  hal/            Hardware-Abstraktion                               │
│                  SPI-Transfer, GPIO                                 │
│                  Plattform-spezifisch                               │
└─────────────────────────────────────────────────────────────────────┘
```

### Anwendung: Was darf wo?

| Schicht    | Darf ✓                               | Darf nicht ✗                    |
|------------|--------------------------------------|---------------------------------|
| `app/`     | Schichten orchestrieren, Timing      | Logik implementieren            |
| `logic/`   | Zustände verarbeiten, Entscheidungen | Pins kennen, SPI aufrufen       |
| `drivers/` | Bytes lesen/schreiben, IC-Protokolle | Debounce-Entscheidungen treffen |
| `hal/`     | Register beschreiben, Plattform-APIs | IC-Protokolle kennen            |

### Herleitung: Abhängigkeits-Graph

```
                    ┌───────────┐
                    │   app/    │
                    │ io_task   │
                    └─────┬─────┘
                          │ ruft auf
            ┌─────────────┼─────────────┐
            │             │             │
            ▼             ▼             ▼
     ┌──────────┐  ┌──────────┐  ┌──────────┐
     │ logic/   │  │ drivers/ │  │ drivers/ │
     │ debounce │  │ cd4021   │  │ hc595    │
     │ selection│  └────┬─────┘  └────┬─────┘
     └──────────┘       │             │
                        └──────┬──────┘
                               ▼
                        ┌──────────┐
                        │   hal/   │
                        │ spi_bus  │
                        └──────────┘

Pfeile zeigen Abhängigkeiten (von oben nach unten)
KEINE Pfeile von unten nach oben!
```

### Anwendung: Testbarkeit durch Schichtentrennung

Die `logic/`-Schicht ist hardware-unabhängig. Du kannst sie auf dem Host-PC testen:

```c
// test_selection.cpp – läuft ohne ESP32, ohne Hardware
#include "logic/selection.h"
#include "unity.h"

void test_selection_toggles_on_repress() {
    SelectionState state = {0};

    // Simuliere: Button 3 zum ersten Mal gedrückt
    bool edge1 = true;
    selection_update(&state, 3, edge1);
    TEST_ASSERT_EQUAL(3, state.selectedId);  // LED 3 an

    // Simuliere: Button 3 nochmal gedrückt → Toggle aus
    bool edge2 = true;
    selection_update(&state, 3, edge2);
    TEST_ASSERT_EQUAL(0, state.selectedId);  // Alle aus
}

void test_selection_switches_on_different_button() {
    SelectionState state = {.selectedId = 3};

    // Button 7 gedrückt (nicht 3)
    selection_update(&state, 7, true);
    TEST_ASSERT_EQUAL(7, state.selectedId);  // LED 7 an, 3 aus
}
```

**Vorteil:** Tests laufen in Sekunden auf dem Entwickler-PC, nicht in Minuten auf dem Target.

### Anwendung: Falsche Schichtverletzungen

```c
// ✗ FALSCH: Logik ruft Hardware direkt auf
// (in logic/selection.cpp)
void selection_update(State* s, uint8_t btn) {
    digitalWrite(PIN_LED_1, HIGH);  // Verboten! Logik kennt keine Pins
}

// ✗ FALSCH: Treiber enthält Geschäftslogik
// (in drivers/cd4021.cpp)
void cd4021_read(uint8_t* raw) {
    spiTransfer(raw, BTN_BYTES);
    if (raw[0] & 0x80) {
        selectedId = 1;  // Verboten! Treiber trifft keine Entscheidungen
    }
}

// ✓ RICHTIG: Klare Trennung
// (in app/io_task.cpp)
void scan_cycle() {
    uint8_t raw[BTN_BYTES];
    cd4021_read(raw);                    // Treiber: nur lesen
    bool edge = debounce_update(raw);    // Logik: Zustand verarbeiten
    selection_update(&state, edge);      // Logik: Entscheidung treffen
    uint8_t leds[LED_BYTES];
    setOneHot(state.selectedId, leds);   // Logik: Output ableiten
    hc595_write(leds);                   // Treiber: nur schreiben
}
```

### Zusammenfassung

> **Merkregel:** „Logik kennt keine Pins. Treiber treffen keine Entscheidungen. app/ orchestriert nur."

---

## 7) Testen ist Pflicht

### Problemstellung

Embedded-Bugs sind schwer zu debuggen. Du siehst nicht, was auf der Leitung passiert. Ohne Oszilloskop oder Logic Analyzer ist Fehlersuche Raterei. Ohne Tests merkst du Regressionen erst, wenn das Gerät falsch reagiert – beim Kunden.

### Regel

Jede Änderung erfordert den passenden Test. Keine Ausnahmen.

### Herleitung: Zweistufige Test-Strategie

| Test-Typ | Befehl                                                 | Testet               | Dauer    |
|----------|--------------------------------------------------------|----------------------|----------|
| Unit     | `pio test -e seeed_xiao_esp32s3_test -v`               | Logik isoliert       | Sekunden |
| E2E      | `pio test -e seeed_xiao_esp32s3_e2e -f test_e2e_hw -v` | Hardware-Integration | Minuten  |

### Anwendung: Wann welcher Test?

| Änderung           | Unit-Test | E2E-Test | Begründung                     |
|--------------------|:---------:|:--------:|--------------------------------|
| Debounce-Threshold |     ✓     |    –     | Reine Logik, keine Hardware    |
| Selection-Policy   |     ✓     |    –     | Reine Logik                    |
| Bit-Mapping-Formel |     ✓     |    ✓     | Logik + Hardware-Verifikation  |
| Pin-Zuordnung      |     –     |    ✓     | Nur mit Hardware verifizierbar |
| SPI-Timing/Mode    |     –     |    ✓     | Hardware-spezifisch            |
| Neuer Kanal-Count  |     ✓     |    ✓     | Logik-Skalierung + HW-Timing   |

### Anwendung: Unit-Test-Beispiele

```c
// test_debounce.cpp
void test_debounce_filters_noise() {
    DebounceState state = {.stable = 0xFF, .counter = 0};
    uint8_t threshold = 3;

    // Simuliere Prellen: 1 → 0 → 1 → 0 → 0 → 0
    TEST_ASSERT_FALSE(debounce_update(&state, 0x7F, threshold));  // counter=1
    TEST_ASSERT_FALSE(debounce_update(&state, 0xFF, threshold));  // counter=0
    TEST_ASSERT_FALSE(debounce_update(&state, 0x7F, threshold));  // counter=1
    TEST_ASSERT_FALSE(debounce_update(&state, 0x7F, threshold));  // counter=2
    TEST_ASSERT_TRUE(debounce_update(&state, 0x7F, threshold));   // counter=3 → Edge!
}

// test_one_hot.cpp
void test_oneHot_clears_all_before_setting() {
    uint8_t ledState[2] = {0xFF, 0xFF};  // Absichtlich "verschmutzt"

    setOneHot(1, ledState, 2);

    TEST_ASSERT_EQUAL_HEX8(0x80, ledState[0]);  // Nur Bit 7
    TEST_ASSERT_EQUAL_HEX8(0x00, ledState[1]);  // Rest gelöscht
}

void test_oneHot_zero_clears_all() {
    uint8_t ledState[2] = {0xFF, 0xFF};

    setOneHot(0, ledState, 2);

    TEST_ASSERT_EQUAL_HEX8(0x00, ledState[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, ledState[1]);
}
```

### Anwendung: E2E-Test-Ablauf

```c
// test_e2e_hw.cpp (interaktiv)
void test_button_to_led_mapping() {
    Serial.println("=== E2E Test: Button-LED-Mapping ===");

    Serial.println("1. Drücke KEINEN Button...");
    delay(1000);
    uint8_t raw[BTN_BYTES];
    cd4021_read(raw);
    TEST_ASSERT_EQUAL_HEX8(0xFF, raw[0]);  // Alle HIGH (Active-Low)
    Serial.println("   ✓ Idle-Zustand OK");

    Serial.println("2. Drücke Button 1...");
    delay(2000);
    cd4021_read(raw);
    TEST_ASSERT_BITS_LOW(0x80, raw[0]);    // Bit 7 = 0
    Serial.println("   ✓ Button 1 erkannt");

    Serial.println("3. Walking-One LEDs...");
    for (int i = 1; i <= LED_COUNT; i++) {
        uint8_t leds[LED_BYTES];
        setOneHot(i, leds, LED_BYTES);
        hc595_write(leds);
        Serial.printf("   LED %d sollte leuchten\n", i);
        delay(500);
    }
    Serial.println("   ✓ Visuelle Prüfung abgeschlossen");
}
```

### Herleitung: Test-Pyramide

```
                    ┌─────────┐
                    │   E2E   │  ← Wenige, langsam, teuer
                    │  Tests  │
                    ├─────────┤
                    │  Unit-  │  ← Viele, schnell, günstig
                    │  Tests  │
                    └─────────┘

Regel: Mehr Unit-Tests, weniger E2E-Tests
       Unit-Tests bei jeder Änderung
       E2E-Tests nach Hardware-Änderungen
```

