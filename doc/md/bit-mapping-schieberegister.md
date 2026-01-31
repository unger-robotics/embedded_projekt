# Bit-Mapping bei Schieberegistern: MSB vs. LSB

## 0) Begriffsrahmen und Zielsetzung

**Kontext:** Warum ordnet der CD4021 die Taster „rückwärts" (Bit 7→0), während der 74HC595 die LEDs „vorwärts" (Bit 0→7) ansteuert? Diese scheinbare Asymmetrie ergibt sich aus der Schieberichtung der beiden Chips.

| Begriff       | Definition                                                            |
|---------------|-----------------------------------------------------------------------|
| **PISO**      | Parallel In, Serial Out – Daten parallel rein, seriell raus (CD4021)  |
| **SIPO**      | Serial In, Parallel Out – Daten seriell rein, parallel raus (74HC595) |
| **Flip-Flop** | Kleinste Speichereinheit, speichert 1 Bit                             |
| **Latch**     | Speicher, der Daten bei Trigger-Signal übernimmt                      |
| **MSBFIRST**  | SPI-Modus: höchstwertiges Bit wird zuerst übertragen                  |

**Ziel:** Du verstehst, warum Taster und LEDs unterschiedliche Maskenformeln brauchen – und kannst die Hardware-Realität im Code korrekt abbilden.

---

## 1) Das Grundprinzip – Wie Schieberegister arbeiten

### 1.1) Was ist ein Flip-Flop?

Ein Flip-Flop ist die kleinste Speichereinheit in der Digitaltechnik – es kann genau ein Bit speichern (0 oder 1). Man kann es sich wie einen Lichtschalter vorstellen: Er ist entweder AN (1) oder AUS (0) und behält seinen Zustand, bis er aktiv umgeschaltet wird.

| Eigenschaft       | Beschreibung                         |
|-------------------|--------------------------------------|
| Speicherkapazität | Genau 1 Bit (0 oder 1)               |
| Zustandsänderung  | Nur bei Taktflanke (Clock)           |
| Verhalten         | Behält Zustand bis zum nächsten Takt |
| Symbol            | D-Flip-Flop (D = Data)               |

### 1.2) Das Schieberegister als Flip-Flop-Kette

Ein Schieberegister ist eine Reihenschaltung von Flip-Flops. Jedes Flip-Flop gibt bei jedem Taktimpuls seinen Inhalt an das nächste weiter:

```
8-Bit Schieberegister = 8 Flip-Flops in Reihe

+------+    +------+    +------+    +------+
| FF 0 |--->| FF 1 |--->| FF 2 |--->| FF 3 |---> ... ---> Ausgang
+------+    +------+    +------+    +------+
   ^
Eingang
```

**Flip-Flops pro Chip:**

| Chip    |  Flip-Flops  | Zusätzlich           |
|---------|:------------:|----------------------|
| CD4021B |      8       | –                    |
| 74HC595 |      8       | + 8 Ausgangs-Latches |
| Kaskade | 13 × 8 = 104 | für 100 LEDs         |

### 1.3) Die Becherkette-Analogie

Bei jedem Taktimpuls wandert jedes Bit um eine Position weiter – wie eine Becherkette, bei der jeder Becher seinen Inhalt an den Nachbarn weitergibt:

```
Becherkette mit 4 Bechern:

Takt 0:  [A] [_] [_] [_]  ← A wird eingefüllt
Takt 1:  [B] [A] [_] [_]  ← A wandert weiter, B kommt nach
Takt 2:  [C] [B] [A] [_]  ← Kette füllt sich
Takt 3:  [D] [C] [B] [A]  ← Kette voll
Takt 4:  [E] [D] [C] [B]  ← A fällt am Ende heraus!
```

> **Merkregel: Schieberegister = Becherkette**
> - Jeder Becher = 1 Flip-Flop = 1 Bit Speicher
> - Jeder Takt = Alle Becher kippen gleichzeitig nach rechts
> - Was links reinkommt, fällt irgendwann rechts raus

Die entscheidende Frage: **Wo kommt das Bit herein, und wo kommt es heraus?**

---

## 2) CD4021 – Vom Taster zum Mikrocontroller

### 2.1) Was der Chip tut

Der CD4021 ist ein Parallel-zu-Seriell-Wandler (PISO: Parallel In, Serial Out). Er hat 8 parallele Eingänge (P1–P8), an denen die Taster hängen. Auf Kommando (PS-Pin = Load) übernimmt er alle 8 Zustände gleichzeitig in sein internes Register. Danach schiebt er sie Bit für Bit über den seriellen Ausgang (Q8) zum ESP32.

### 2.2) Die Hardware-Realität

Schauen wir ins Datenblatt: Der CD4021 gibt **P8 zuerst** aus, dann P7, dann P6 … und P1 zuletzt. Die interne Schiebekette sieht so aus:

```
Schieberichtung: →

+-----+   +-----+   +-----+   +-----+   +-----+   +-----+   +-----+   +-----+
| P1  |-->| P2  |-->| P3  |-->| P4  |-->| P5  |-->| P6  |-->| P7  |-->| P8  |--> Q8
+-----+   +-----+   +-----+   +-----+   +-----+   +-----+   +-----+   +-----+
                                                                           ^
                                                                  Serieller Ausgang
```

Nach dem LOAD-Signal liegt P8 direkt am Ausgang Q8 bereit. Mit jedem Taktimpuls rückt die Kette nach rechts:

| Takt | Bit an Q8 | Position im empfangenen Byte   |
|:----:|-----------|--------------------------------|
|  1   | P8        | Bit 7 (MSB, zuerst empfangen)  |
|  2   | P7        | Bit 6                          |
|  3   | P6        | Bit 5                          |
|  4   | P5        | Bit 4                          |
|  5   | P4        | Bit 3                          |
|  6   | P3        | Bit 2                          |
|  7   | P2        | Bit 1                          |
|  8   | P1        | Bit 0 (LSB, zuletzt empfangen) |

> **Ergebnis:** Das empfangene Byte hat das Format `[P8][P7][P6][P5][P4][P3][P2][P1]` – das höchstwertige Bit (MSB) kam zuerst.

### 2.3) Die Verdrahtungskonvention im Projekt

Die Taster sind so angeschlossen, dass Taster 1 an P8 hängt, Taster 8 an P1:

```
Physische Verdrahtung:

+--------+--------+--------+--------+--------+--------+--------+--------+
|Taster 1|Taster 2|Taster 3|Taster 4|Taster 5|Taster 6|Taster 7|Taster 8|
+---+----+---+----+---+----+---+----+---+----+---+----+---+----+---+----+
    |        |        |        |        |        |        |        |
    v        v        v        v        v        v        v        v
   P8       P7       P6       P5       P4       P3       P2       P1
```

**Warum diese Verdrahtung?** Damit die Taster-Nummer direkt der Bit-Position im empfangenen Byte entspricht:

- Taster 1 → P8 → kommt zuerst raus → landet auf Bit 7
- Taster 2 → P7 → kommt als zweites → landet auf Bit 6
- …
- Taster 8 → P1 → kommt zuletzt → landet auf Bit 0

### 2.4) Die Formel im Code

```c
// id = 1..8, wir wollen Bit 7..0
uint8_t btnMask_msb(uint8_t id) {
    return 1u << (7 - ((id - 1) % 8));
}
```

**Schritt für Schritt für Taster 3:**

1. `id = 3`
2. `(id - 1) = 2` → nullbasierter Index
3. `(id - 1) % 8 = 2` → Position innerhalb eines Bytes
4. `7 - 2 = 5` → Bit-Position (von MSB gezählt)
5. `1 << 5 = 0b0010.0000 = 0x20` → Maske

### 2.5) Maskenberechnung für Taster 1–10

**Byte 0 (Taster 1–8):**

| Button | ID | (id-1) % 8 | 7 - … | Binär (nibbleweise) | Hex    |
|-------:|---:|-----------:|------:|---------------------|--------|
|      1 |  1 |          0 |     7 | `0b1000.0000`       | `0x80` |
|      2 |  2 |          1 |     6 | `0b0100.0000`       | `0x40` |
|      3 |  3 |          2 |     5 | `0b0010.0000`       | `0x20` |
|      4 |  4 |          3 |     4 | `0b0001.0000`       | `0x10` |
|      5 |  5 |          4 |     3 | `0b0000.1000`       | `0x08` |
|      6 |  6 |          5 |     2 | `0b0000.0100`       | `0x04` |
|      7 |  7 |          6 |     1 | `0b0000.0010`       | `0x02` |
|      8 |  8 |          7 |     0 | `0b0000.0001`       | `0x01` |

**Byte 1 (Taster 9–10):**

| Button | ID | Byte | Bit | Maske  |
|-------:|---:|-----:|----:|--------|
|      9 |  9 |    1 |   7 | `0x80` |
|     10 | 10 |    1 |   6 | `0x40` |

---

## 3) 74HC595 – Vom Mikrocontroller zur LED

### 3.1) Was der Chip tut

Der 74HC595 ist ein Seriell-zu-Parallel-Wandler (SIPO: Serial In, Parallel Out) – das Gegenstück zum CD4021. Er empfängt Bits seriell vom ESP32 und gibt sie parallel an 8 Ausgängen (QA–QH) aus.

### 3.2) Die Hardware-Realität

Das **erste gesendete Bit** wandert durch die gesamte Kette und landet am Ende auf QH. Das **letzte Bit** bleibt am Anfang auf QA.

```
Dateneingang DS
      |
      v
+-----+   +-----+   +-----+   +-----+   +-----+   +-----+   +-----+   +-----+
| QA  |<--| QB  |<--| QC  |<--| QD  |<--| QE  |<--| QF  |<--| QG  |<--| QH  |
+--+--+   +--+--+   +--+--+   +--+--+   +--+--+   +--+--+   +--+--+   +--+--+
   |         |         |         |         |         |         |         |
   v         v         v         v         v         v         v         v
 LED 1     LED 2     LED 3     LED 4     LED 5     LED 6     LED 7     LED 8
```

Wenn wir ein Byte mit MSBFIRST senden:

| Takt | Gesendetes Bit | Wandert nach |
|:----:|----------------|--------------|
|  1   | Bit 7 (MSB)    | → QH         |
|  2   | Bit 6          | → QG         |
|  3   | Bit 5          | → QF         |
|  4   | Bit 4          | → QE         |
|  5   | Bit 3          | → QD         |
|  6   | Bit 2          | → QC         |
|  7   | Bit 1          | → QB         |
|  8   | Bit 0 (LSB)    | → QA         |

> **Ergebnis:** Bit 0 landet auf QA, Bit 7 landet auf QH.

### 3.3) Die Verdrahtungskonvention im Projekt

Die LEDs hängen so, dass LED 1 an QA, LED 8 an QH:

- LED 1 → QA → gesteuert durch Bit 0
- LED 2 → QB → gesteuert durch Bit 1
- …
- LED 8 → QH → gesteuert durch Bit 7

### 3.4) Die Formel im Code

```c
// id = 1..10, wir wollen Bit 0..9
void setOneHot(uint8_t selected_id) {
    uint8_t bit = selected_id - 1;              // 0..9
    ledState[bit / 8] |= (1u << (bit % 8));
}
```

**Schritt für Schritt für LED 5:**

1. `selected_id = 5`
2. `bit = 5 - 1 = 4` → nullbasierter Index
3. `bit / 8 = 0` → Byte-Index (erstes Byte)
4. `bit % 8 = 4` → Bit-Position innerhalb des Bytes
5. `1 << 4 = 0b0001.0000 = 0x10` → Maske

### 3.5) Maskenberechnung für LEDs 1–10

| LED | ID | bit | Byte | Bit-Pos | Binär (nibbleweise) | Hex    |
|----:|---:|----:|-----:|--------:|---------------------|--------|
|   1 |  1 |   0 |    0 |       0 | `0b0000.0001`       | `0x01` |
|   2 |  2 |   1 |    0 |       1 | `0b0000.0010`       | `0x02` |
|   3 |  3 |   2 |    0 |       2 | `0b0000.0100`       | `0x04` |
|   4 |  4 |   3 |    0 |       3 | `0b0000.1000`       | `0x08` |
|   5 |  5 |   4 |    0 |       4 | `0b0001.0000`       | `0x10` |
|   6 |  6 |   5 |    0 |       5 | `0b0010.0000`       | `0x20` |
|   7 |  7 |   6 |    0 |       6 | `0b0100.0000`       | `0x40` |
|   8 |  8 |   7 |    0 |       7 | `0b1000.0000`       | `0x80` |
|   9 |  9 |   8 |    1 |       0 | `0b0000.0001`       | `0x01` |
|  10 | 10 |   9 |    1 |       1 | `0b0000.0010`       | `0x02` |

---

## 4) Warum die Asymmetrie?

Die unterschiedliche Bit-Reihenfolge ist kein Fehler – sie ergibt sich aus der **Schieberichtung** der beiden Chips:

| Chip    | Typ  | Schieberichtung  | Erstes Bit       | Letztes Bit      |
|---------|------|------------------|------------------|------------------|
| CD4021  | PISO | P1 → P2 → … → Q8 | P8 → Bit 7 (MSB) | P1 → Bit 0 (LSB) |
| 74HC595 | SIPO | DS → QH → … → QA | Bit 7 → QH       | Bit 0 → QA       |

Beide Chips verwenden **MSBFIRST** als SPI-Modus. Das bedeutet:

- **CD4021:** Der Chip gibt MSB zuerst aus → wir empfangen MSB zuerst → BTN1 liegt auf Bit 7
- **74HC595:** Wir senden MSB zuerst → MSB wandert nach hinten (QH) → LED1 braucht Bit 0

---

## 5) Zusammenfassung: Das Bit-Layout

### 5.1) CD4021 (Taster einlesen) – MSB first

```
Byte 0:  [BTN1][BTN2][BTN3][BTN4][BTN5][BTN6][BTN7][BTN8]
          Bit7  Bit6  Bit5  Bit4  Bit3  Bit2  Bit1  Bit0
         (MSB)                                     (LSB)

Byte 1:  [BTN9][BTN10][ - ][ - ][ - ][ - ][ - ][ - ]
          Bit7  Bit6              (ungenutzt)
```

### 5.2) 74HC595 (LEDs ansteuern) – LSB mapping

```
Byte 0:  [LED1][LED2][LED3][LED4][LED5][LED6][LED7][LED8]
          Bit0  Bit1  Bit2  Bit3  Bit4  Bit5  Bit6  Bit7
         (LSB)                                     (MSB)

Byte 1:  [LED9][LED10][ - ][ - ][ - ][ - ][ - ][ - ]
          Bit0  Bit1              (ungenutzt)
```

### 5.3) Die Formeln auf einen Blick

```c
// Taster: MSB-first → Bit 7 zuerst
uint8_t btnMask = 1u << (7 - ((id - 1) % 8));
uint8_t btnByte = (id - 1) / 8;

// LEDs: LSB-mapping → Bit 0 zuerst
uint8_t ledMask = 1u << ((id - 1) % 8);
uint8_t ledByte = (id - 1) / 8;
```

### 5.4) Visualisierung der Asymmetrie

```
                CD4021 (Input)                    74HC595 (Output)
                PISO: Parallel In, Serial Out     SIPO: Serial In, Parallel Out
                Schieberichtung →                 ← Schieberichtung

    P1   P2   P3   P4   P5   P6   P7   P8         QA   QB   QC   QD   QE   QF   QG   QH
    ↓    ↓    ↓    ↓    ↓    ↓    ↓    ↓          ↑    ↑    ↑    ↑    ↑    ↑    ↑    ↑
┌────────────────────────────────────────┐   ┌────────────────────────────────────────┐
│ Taster 1  2  3  4  5  6  7  8          │   │ Byte 0   1  2  3  4  5  6  7  8        │
│                                    → Q8│   │ Bit0  Bit1 ... ... ... ... Bit6 Bit7  │
└────────────────────────────────────────┘   └────────────────────────────────────────┘
         ↓                                            ↓
┌────────────────────────────────────────┐   ┌────────────────────────────────────────┐
│ Byte 0   1  2  3  4  5  6  7  8        │   │ LEDs   1  2  3  4  5  6  7  8          │
│ Bit7  Bit6 ... ... ... ... Bit1 Bit0   │   │       QA QB QC QD QE QF QG QH          │
│ (MSB)                          (LSB)   │   │      (LSB)                   (MSB)     │
└────────────────────────────────────────┘   └────────────────────────────────────────┘

P8 kommt zuerst raus → landet auf Bit 7     Bit 0 landet auf QA → LED1 braucht Bit 0
btnMask = 1 << (7 - ((id-1) % 8))           ledMask = 1 << ((id-1) % 8)
```

> **Fazit:** Die Software kompensiert die Hardware-bedingte Asymmetrie durch unterschiedliche Masken-Berechnungen – und am Ende passt Taster i zu LED i.

---

## 6) Praktische Implementierung

### 6.1) Button prüfen (Active-Low)

```c
bool isButtonPressed(uint8_t id, const uint8_t* raw) {
    if (id == 0 || id > BTN_COUNT) return false;

    uint8_t byteIdx = (id - 1) / 8;
    uint8_t bitPos  = 7 - ((id - 1) % 8);   // MSB-first!

    // Active-low: gedrückt wenn Bit = 0
    return (raw[byteIdx] & (1u << bitPos)) == 0;
}
```

### 6.2) LED setzen/löschen

```c
void setLed(uint8_t id) {
    if (id == 0 || id > LED_COUNT) return;

    uint8_t byteIdx = (id - 1) / 8;
    uint8_t bitPos  = (id - 1) % 8;         // LSB-mapping

    ledState[byteIdx] |= (1u << bitPos);
}

void clearLed(uint8_t id) {
    if (id == 0 || id > LED_COUNT) return;

    uint8_t byteIdx = (id - 1) / 8;
    uint8_t bitPos  = (id - 1) % 8;

    ledState[byteIdx] &= ~(1u << bitPos);
}
```

### 6.3) One-Hot setzen (nur eine LED an)

```c
void setOneHot(uint8_t selectedId, uint8_t* out, size_t bytes) {
    memset(out, 0, bytes);   // Alle Bits löschen

    if (selectedId >= 1 && selectedId <= LED_COUNT) {
        uint8_t byteIdx = (selectedId - 1) / 8;
        uint8_t bitPos  = (selectedId - 1) % 8;
        out[byteIdx] = 1u << bitPos;
    }
}
```

---

## 7) Last-Byte-Maskierung

**Problem:** Bei 10 LEDs brauchst du 2 Bytes (16 Bits), aber nur 10 sind gültig. Die Bits 2–7 im zweiten Byte sind „überzählig".

**Berechnung:**

```c
uint8_t validBits = LED_COUNT % 8;
if (validBits == 0) validBits = 8;

// LSB-mapping: die untersten validBits sind gültig
uint8_t LED_LAST_MASK = (1u << validBits) - 1;

// MSB-first: die obersten validBits sind gültig
uint8_t BTN_LAST_MASK = 0xFF << (8 - validBits);
```

| Count | validBits | LED_LAST_MASK | BTN_LAST_MASK | Gültige Bits         |
|------:|----------:|--------------:|--------------:|----------------------|
|     8 |         8 |        `0xFF` |        `0xFF` | alle                 |
|    10 |         2 |        `0x03` |        `0xC0` | Bit 0,1 bzw. Bit 7,6 |
|    12 |         4 |        `0x0F` |        `0xF0` | Bit 0–3 bzw. Bit 7–4 |
|   100 |         4 |        `0x0F` |        `0xF0` | Bit 0–3 bzw. Bit 7–4 |

**Anwendung:**

```c
// LEDs: ungültige Bits auf 0 setzen (bevor an HC595 gesendet)
ledState[LED_BYTES - 1] &= LED_LAST_MASK;

// Buttons: ungültige Bits auf 1 setzen (active-low idle)
raw[BTN_BYTES - 1] |= ~BTN_LAST_MASK;
```

