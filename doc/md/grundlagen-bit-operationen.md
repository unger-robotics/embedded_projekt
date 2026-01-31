# Grundlagen: Zahlensysteme und Bit-Operationen

## 0) Begriffsrahmen und Zielsetzung

**Anwendungskontext:** Schieberegister (CD4021B für Eingabe, 74HC595 für Ausgabe) verarbeiten Daten byteweise. Die Firmware muss einzelne Bits gezielt lesen und schreiben können – etwa um festzustellen, ob Taster 7 gedrückt ist, oder um LED 5 einzuschalten.

**Lernziele:** Nach diesem Kapitel kannst du:

1. Zahlenwerte zwischen Dezimal, Binär und Hexadezimal umrechnen
2. Den passenden Datentyp für eine gegebene Bitbreite wählen
3. Bit-Operationen (Setzen, Löschen, Prüfen, Toggeln) sicher anwenden
4. Eine Button-ID in Byte-Index und Bit-Position überführen

**Begriffsdefinitionen:**

| Begriff    | Definition                                                    | Quelle/Norm              |
|------------|---------------------------------------------------------------|--------------------------|
| **Bit**    | Kleinste Informationseinheit; Wertebereich {0, 1}             | IEC 60027-2              |
| **Byte**   | Gruppe von 8 Bits; Wertebereich 0–255 (unsigned)              | IEC 80000-13             |
| **Nibble** | Gruppe von 4 Bits; entspricht einer Hexadezimalziffer         | Gebräuchliche Konvention |
| **MSB**    | Most Significant Bit – Bit 7, höchster Stellenwert (2⁷=128)   | –                        |
| **LSB**    | Least Significant Bit – Bit 0, niedrigster Stellenwert (2⁰=1) | –                        |
| **Maske**  | Bit-Muster zur gezielten Manipulation bestimmter Bits         | –                        |

---

## 1) Zahlensysteme – Drei Darstellungen, ein Wert

**Problemstellung:** Der Wert „zwölf" lässt sich auf verschiedene Arten schreiben. In der Embedded-Entwicklung begegnen alle drei Darstellungen – oft im selben Quellcode. Das Verständnis der Umrechnung ist Voraussetzung für die Arbeit mit Bit-Masken.

### 1.1) Die drei Zahlensysteme

| System      | Basis | Ziffernvorrat | C/C++ Präfix | Beispiel (Wert 12) |
|-------------|:-----:|---------------|:------------:|--------------------|
| Dezimal     |  10   | 0–9           |   (keiner)   | `12`               |
| Binär       |   2   | 0, 1          |     `0b`     | `0b0000.1100`      |
| Hexadezimal |  16   | 0–9, A–F      |     `0x`     | `0x0C`             |

**Anwendungsregel – Welches System wann?**

| Situation                       | Empfohlenes System | Beispiel              |
|---------------------------------|--------------------|-----------------------|
| Zählwerte, Indizes, Zeitangaben | Dezimal            | `count = 10`          |
| Einzelne Bits sichtbar machen   | Binär              | `0b1000.0000` (Bit 7) |
| Byte-Werte, Adressen, Register  | Hexadezimal        | `0x80` (kompakt)      |

### 1.2) Nibble-Prinzip – Binär und Hex verbinden

Ein Nibble umfasst 4 Bits und entspricht exakt einer Hexadezimalziffer. Diese Korrespondenz ermöglicht schnelle Umrechnungen ohne Taschenrechner.

**Schreibkonvention:** Zur besseren Lesbarkeit trennen wir Nibbles mit einem Punkt:

```
0b0001.0000  statt  0b00010000
  ~~~~.~~~~
  High Low
  Nibble
```

**Umrechnungstabelle (Nibble ↔ Hex):**

| Hex | Binär  | Dez |   | Hex | Binär  | Dez |
|:---:|:------:|----:|---|:---:|:------:|----:|
|  0  | `0000` |   0 |   |  8  | `1000` |   8 |
|  1  | `0001` |   1 |   |  9  | `1001` |   9 |
|  2  | `0010` |   2 |   |  A  | `1010` |  10 |
|  3  | `0011` |   3 |   |  B  | `1011` |  11 |
|  4  | `0100` |   4 |   |  C  | `1100` |  12 |
|  5  | `0101` |   5 |   |  D  | `1101` |  13 |
|  6  | `0110` |   6 |   |  E  | `1110` |  14 |
|  7  | `0111` |   7 |   |  F  | `1111` |  15 |

*Diese Tabelle sollte auswendig beherrscht werden – sie ist das Einmaleins der Bit-Manipulation.*

### 1.3) Anwendungsbeispiel: 0x80 in Binär umwandeln

**Aufgabe:** Wandle `0x80` in Binärdarstellung um.

**Schritt-für-Schritt-Herleitung:**

```
Schritt 1: Hex-Ziffern identifizieren
    0x80 = 0x8 und 0x0

Schritt 2: Jede Hex-Ziffer → Nibble (aus Tabelle)
    0x8 → 0b1000
    0x0 → 0b0000

Schritt 3: Nibbles zusammensetzen
    0x80 = 0b1000.0000

Schritt 4: Interpretation
    Nur Bit 7 ist gesetzt → Stellenwert 2⁷ = 128
```

**Ergebnis:** `0x80 = 0b1000.0000 = 128₁₀`

Diese Maske adressiert Bit 7 – im MSB-first-Mapping entspricht das ID 1.

---

## 2) Datentypen mit fester Bitbreite

**Problemstellung:** Die C/C++-Standardtypen (`int`, `char`) haben plattformabhängige Größen. Ein `int` kann 16, 32 oder 64 Bit breit sein. Für Schieberegister, die exakt 8 Bit verarbeiten, ist diese Unbestimmtheit problematisch.

**Lösung:** Der Header `<stdint.h>` (C) bzw. `<cstdint>` (C++) definiert Typen mit garantierter Bitbreite (vgl. ISO/IEC 9899:1999, §7.18).

### 2.1) Integer-Typen mit garantierter Breite

| Typ        | Bits | Wertebereich (dezimal)            | Wertebereich (hex)          |
|------------|:----:|-----------------------------------|-----------------------------|
| `uint8_t`  |  8   | 0 bis 255                         | `0x00` – `0xFF`             |
| `int8_t`   |  8   | −128 bis +127                     | `0x80` – `0x7F`             |
| `uint16_t` |  16  | 0 bis 65.535                      | `0x0000` – `0xFFFF`         |
| `int16_t`  |  16  | −32.768 bis +32.767               | `0x8000` – `0x7FFF`         |
| `uint32_t` |  32  | 0 bis 4.294.967.295               | `0x00000000` – `0xFFFFFFFF` |
| `int32_t`  |  32  | −2.147.483.648 bis +2.147.483.647 | `0x80000000` – `0x7FFFFFFF` |

**Namenskonvention dekodieren:**

| Bestandteil | Bedeutung                    | Beispiel        |
|-------------|------------------------------|-----------------|
| `u`         | unsigned (vorzeichenlos)     | `uint8_t`       |
| Zahl        | Bitbreite                    | `8`, `16`, `32` |
| `_t`        | type (Typedef-Kennzeichnung) | –               |

*Lesehilfe:* `uint8_t` = **u**nsigned **int**eger, **8** Bit, **t**ype

### 2.2) Typwahl für Schieberegister

Ein Schieberegister (CD4021B, 74HC595) verarbeitet exakt 8 Bits. Der Typ `uint8_t` bildet dies 1:1 ab:

```c
#include <stdint.h>

uint8_t buttonState = 0;           // 8 Taster → 1 Byte
uint8_t ledState[2] = {0, 0};      // 10 LEDs  → 2 Bytes (16 Bits, 10 genutzt)
```

**Typvergleich für Schieberegister-Anwendungen:**

| Typ             | Bits  | Garantiert?           | Bewertung                             |
|-----------------|:-----:|-----------------------|---------------------------------------|
| `uint8_t`       |   8   | Ja, exakt 8 Bit       | **Empfohlen** – explizit, portabel    |
| `unsigned char` |   8   | Mindestens 8 Bit      | Funktioniert, aber weniger lesbar     |
| `byte`          |   8   | Nur Arduino-Framework | Nicht portabel auf andere Plattformen |
| `int`           | 16–32 | Plattformabhängig     | Ungeeignet – verschwendet RAM         |

### 2.3) Überlauf (Overflow) und Unterlauf (Underflow)

Bei unsigned-Typen „wickelt" der Wert bei Bereichsüberschreitung um – ohne Fehlermeldung:

```c
uint8_t x = 255;
x = x + 1;    // Ergebnis: x = 0   (nicht 256!)

uint8_t y = 0;
y = y - 1;    // Ergebnis: y = 255 (nicht -1!)
```

**Visualisierung als Zahlenkreis:**

```
             0
         255 ↑ 1
       254 ←─┼─→ 2
           · · ·
       129 ←─┼─→ 127
         128
```

*Bei 255+1 springt der Wert auf 0; bei 0−1 springt er auf 255.*

**Anwendungshinweis:** Dieses Verhalten ist definiert (kein Undefined Behavior bei unsigned), kann aber zu schwer auffindbaren Fehlern führen. Prüfe Grenzwerte vor arithmetischen Operationen.

### 2.4) Das Suffix `u` – Warum `1u` statt `1`?

**Problemstellung:** Das Literal `1` hat den Typ `int` (signed). Bei Shift-Operationen kann dies zu undefiniertem Verhalten führen.

**Hintergrund – signed vs. unsigned bei Shifts:**

```
int8_t (signed, 8 Bit):
┌───┬───┬───┬───┬───┬───┬───┬───┐
│ V │ 6 │ 5 │ 4 │ 3 │ 2 │ 1 │ 0 │   V = Vorzeichen-Bit
└───┴───┴───┴───┴───┴───┴───┴───┘
  │
  └── 0 = positiv (+0 bis +127)
      1 = negativ (−128 bis −1)

uint8_t (unsigned, 8 Bit):
┌───┬───┬───┬───┬───┬───┬───┬───┐
│ 7 │ 6 │ 5 │ 4 │ 3 │ 2 │ 1 │ 0 │   Alle 8 Bits für den Wert
└───┴───┴───┴───┴───┴───┴───┴───┘
```

**Konkretes Fehlerszenario:**

```c
// OHNE 'u' – problematisch:
int result = 1 << 31;     // Undefiniertes Verhalten! (signed overflow)

// MIT 'u' – sicher:
unsigned int result = 1u << 31;   // OK, definiertes Verhalten
```

**Regel:** Ein Linksshift, der das Vorzeichen-Bit eines signed Typs modifiziert, ist laut C++-Standard (ISO/IEC 14882:2017, §8.8) undefiniert. Der Compiler darf beliebigen Code erzeugen.

**Best Practice – Immer `1u` bei Bit-Operationen:**

```c
uint8_t mask = 1u << 7;           // ✓ Sicher
uint8_t mask = 1 << 7;            // ✗ Riskant (auf manchen Plattformen)

ledState |= (1u << bitPos);       // ✓ Setzen
ledState &= ~(1u << bitPos);      // ✓ Löschen
```

> **Merkregel:** Siehst du `<<` oder `>>`, schreib `1u` statt `1`.

---

## 3) Skalierung – Von 8 auf 100+ Bits

**Problemstellung:** Standard-Integer-Typen enden bei 64 Bit. Für 100 LEDs benötigen wir 100 Bits – wie organisieren wir diese?

### 3.1) Verfügbare Integer-Typen

| Typ        | Bits | Status                   |
|------------|:----:|--------------------------|
| `uint8_t`  |  8   | ✓ Standard               |
| `uint16_t` |  16  | ✓ Standard               |
| `uint32_t` |  32  | ✓ Standard               |
| `uint64_t` |  64  | ✓ Standard (**Maximum**) |

Der ESP32-S3 als 32-Bit-Prozessor kann maximal 32 Bits in einem Rechenschritt verarbeiten. Für größere Bitmengen verwenden wir Arrays.

### 3.2) Lösung: Byte-Arrays

**Formel – Anzahl Bytes berechnen:**

$$\text{Bytes} = \left\lceil \frac{\text{Bits}}{8} \right\rceil = \frac{\text{Bits} + 7}{8} \quad \text{(ganzzahlige Division)}$$

**Herleitung am Beispiel:**

```
Gegeben: 100 LEDs → 100 Bits benötigt

Rechnung: (100 + 7) / 8 = 107 / 8 = 13  (ganzzahlig)

Ergebnis: 13 Bytes (= 104 Bits, davon 100 genutzt, 4 ungenutzt)
```

**Umsetzung in C:**

```c
#define NUM_LEDS  100
#define NUM_BYTES ((NUM_LEDS + 7) / 8)   // = 13

uint8_t ledState[NUM_BYTES] = {0};       // 13 Bytes, alle auf 0 initialisiert
```

### 3.3) Adressierung: ID → Byte-Index und Bit-Position

Um ein bestimmtes Bit in einem Array zu adressieren, benötigen wir zwei Informationen:

1. **Byte-Index:** In welchem Array-Element liegt das Bit?
2. **Bit-Position:** Welches Bit innerhalb dieses Bytes?

**Formeln (für 1-basierte IDs):**

```c
uint8_t byteIndex   = (id - 1) / 8;      // Ganzzahlige Division
uint8_t bitPosition = (id - 1) % 8;      // Modulo (Rest)
uint8_t mask        = 1u << bitPosition; // Bit-Maske
```

### 3.4) Anwendungsbeispiel: LED 87 einschalten

**Schritt-für-Schritt-Herleitung:**

```
Gegeben: id = 87

Schritt 1: Nullbasierter Index
    bit = id - 1 = 87 - 1 = 86

Schritt 2: Byte-Index (welches Byte im Array?)
    byteIndex = 86 / 8 = 10  (ganzzahlig)
    → LED 87 liegt in ledState[10]

Schritt 3: Bit-Position (welches Bit im Byte?)
    bitPosition = 86 % 8 = 6
    → LED 87 liegt auf Bit 6

Schritt 4: Maske erzeugen
    mask = 1u << 6 = 0b0100.0000 = 0x40

Schritt 5: Bit setzen
    ledState[10] |= 0x40;
```

**Kontrolltabelle für ausgewählte IDs:**

| LED | id−1 | Byte | Bit | Maske (binär) | Maske (hex) |
|----:|-----:|-----:|----:|---------------|-------------|
|   1 |    0 |    0 |   0 | `0b0000.0001` | `0x01`      |
|   8 |    7 |    0 |   7 | `0b1000.0000` | `0x80`      |
|   9 |    8 |    1 |   0 | `0b0000.0001` | `0x01`      |
|  16 |   15 |    1 |   7 | `0b1000.0000` | `0x80`      |
|  50 |   49 |    6 |   1 | `0b0000.0010` | `0x02`      |
|  87 |   86 |   10 |   6 | `0b0100.0000` | `0x40`      |
| 100 |   99 |   12 |   3 | `0b0000.1000` | `0x08`      |

*Beobachtung:* Bei jeder 8. LED (8, 16, 24, …) springt der Byte-Index um 1, die Bit-Position kehrt zu 0 zurück.

---

## 4) Bit-Positionen und Stellenwerte

**Definition:** Ein Byte besteht aus 8 Bits mit den Positionen 0 bis 7. Jede Position hat einen Stellenwert, der einer Zweierpotenz entspricht.

### 4.1) Aufbau eines Bytes

```
Position:      7      6      5      4      3      2      1      0
             ┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐
Byte:        │      │      │      │      │      │      │      │      │
             └──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┘
Stellenwert:   128    64     32     16      8      4      2      1
Potenz:        2⁷     2⁶     2⁵     2⁴     2³     2²     2¹     2⁰
               MSB                                              LSB
```

**Stellenwert-Formel:** Bit n hat den Wert $2^n$.

### 4.2) Referenztabelle: Bit-Position → Maske

| Bit | Stellenwert | Berechnung | Maske (binär) | Maske (hex) |
|:---:|------------:|:----------:|---------------|:-----------:|
|  0  |           1 | `1u << 0`  | `0b0000.0001` |   `0x01`    |
|  1  |           2 | `1u << 1`  | `0b0000.0010` |   `0x02`    |
|  2  |           4 | `1u << 2`  | `0b0000.0100` |   `0x04`    |
|  3  |           8 | `1u << 3`  | `0b0000.1000` |   `0x08`    |
|  4  |          16 | `1u << 4`  | `0b0001.0000` |   `0x10`    |
|  5  |          32 | `1u << 5`  | `0b0010.0000` |   `0x20`    |
|  6  |          64 | `1u << 6`  | `0b0100.0000` |   `0x40`    |
|  7  |         128 | `1u << 7`  | `0b1000.0000` |   `0x80`    |

> **Merkregel:** Die Hex-Werte folgen dem Muster 1, 2, 4, 8 pro Nibble:
> - Low-Nibble (Bit 0–3): `0x01`, `0x02`, `0x04`, `0x08`
> - High-Nibble (Bit 4–7): `0x10`, `0x20`, `0x40`, `0x80`

### 4.3) Anwendungsbeispiel: Bytewert aus gesetzten Bits berechnen

**Aufgabe:** Welchen dezimalen Wert hat `0b0100.1010`?

**Herleitung:**

```
Byte:          0  1  0  0  .  1  0  1  0
Position:      7  6  5  4     3  2  1  0
               │  │           │     │
               │  └── Bit 6   │     └── Bit 1 gesetzt
               │      gesetzt │
               │              └── Bit 3 gesetzt
               └── Bit 7 nicht gesetzt

Gesetzte Bits: 6, 3, 1
Wert = 2⁶ + 2³ + 2¹ = 64 + 8 + 2 = 74

Kontrolle in Hex: 0b0100.1010 = 0x4A
    0x4 = 4₁₀ × 16 = 64
    0xA = 10₁₀
    Summe: 64 + 10 = 74 ✓
```

---

## 5) Der Modulo-Operator (%)

**Definition:** Der Modulo-Operator berechnet den Rest einer ganzzahligen Division.

$$a \mod b = a - \left\lfloor \frac{a}{b} \right\rfloor \cdot b$$

### 5.1) Grundprinzip

**Beispiel:** `13 % 5`

```
Rechnung: 13 ÷ 5 = 2 Rest 3
          │       │      │
          │       │      └── Modulo-Ergebnis
          │       └── Ganzzahliger Quotient
          └── Dividend

Probe: 2 × 5 + 3 = 10 + 3 = 13 ✓
```

### 5.2) Spezialfall: Dividend < Divisor

**Frage:** Was ergibt `4 % 8`?

```
Rechnung: 4 ÷ 8 = 0 Rest 4
          │       │      │
          │       │      └── Das gesamte 4 bleibt als Rest
          │       └── 0-mal passt 8 in 4
          └── Dividend

Ergebnis: 4 % 8 = 4
```

> **Regel:** Ist der Dividend kleiner als der Divisor, ist das Ergebnis der Dividend selbst.

### 5.3) Anwendung: Position innerhalb eines Bytes

Der Ausdruck `bit % 8` liefert die Position innerhalb eines Bytes (0–7):

| bit | bit / 8 | bit % 8 | Interpretation          |
|----:|--------:|--------:|-------------------------|
|   0 |       0 |       0 | Byte 0, Bit 0           |
|   4 |       0 |       4 | Byte 0, Bit 4           |
|   7 |       0 |       7 | Byte 0, Bit 7           |
|   8 |       1 |       0 | Byte 1, Bit 0 (Sprung!) |
|   9 |       1 |       1 | Byte 1, Bit 1           |
|  15 |       1 |       7 | Byte 1, Bit 7           |
|  16 |       2 |       0 | Byte 2, Bit 0 (Sprung!) |

*Beobachtung:* Bei jedem Vielfachen von 8 springt `bit % 8` auf 0 zurück – der Übergang zum nächsten Byte.

---

## 6) Der Linksshift-Operator (<<)

**Definition:** Der Operator `<<` verschiebt alle Bits um n Positionen nach links. Rechts werden Nullen nachgefüllt.

**Syntax:** `a << n` – „a um n nach links shiften"

### 6.1) Visualisierung

```
Beispiel: 1 << 4

Start:    0b0000.0001  (Wert 1)
               │
               └── Einziges gesetztes Bit auf Position 0

Shift:    ←←←← (4 Positionen)

Ergebnis: 0b0001.0000  (Wert 16)
               │
               └── Bit jetzt auf Position 4
```

### 6.2) Schrittweise Ausführung: `1 << 4`

| Schritt | Operation | Binär         | Dezimal | Potenz |
|:-------:|-----------|---------------|--------:|--------|
|    0    | Start     | `0b0000.0001` |       1 | 2⁰     |
|    1    | `1 << 1`  | `0b0000.0010` |       2 | 2¹     |
|    2    | `1 << 2`  | `0b0000.0100` |       4 | 2²     |
|    3    | `1 << 3`  | `0b0000.1000` |       8 | 2³     |
|    4    | `1 << 4`  | `0b0001.0000` |      16 | 2⁴     |

### 6.3) Mathematischer Zusammenhang

Linksshift um n entspricht Multiplikation mit 2ⁿ:

$$a \ll n = a \times 2^n$$

**Beispiel:** $1 \ll 4 = 1 \times 2^4 = 1 \times 16 = 16$

*Analogie:* Im Dezimalsystem entspricht das Anhängen einer Null der Multiplikation mit 10. Im Binärsystem entspricht Linksshift um 1 der Multiplikation mit 2.

*Grenze der Analogie:* Anders als beim Dezimalsystem gehen beim Shift Bits verloren, die über den Wertebereich hinausschieben (bei 8-Bit: Bit 7 und höher).

---

## 7) Der Rechtsshift-Operator (>>)

**Definition:** Der Operator `>>` verschiebt alle Bits um n Positionen nach rechts. Links werden Nullen nachgefüllt (bei unsigned Typen).

**Syntax:** `a >> n` – „a um n nach rechts shiften"

### 7.1) Visualisierung

```
Beispiel: 16 >> 2

Start:    0b0001.0000  (Wert 16)
               │
               └── Bit auf Position 4

Shift:    →→ (2 Positionen)

Ergebnis: 0b0000.0100  (Wert 4)
                  │
                  └── Bit jetzt auf Position 2
```

### 7.2) Schrittweise Ausführung: `16 >> n`

| Schritt | Operation | Binär         | Dezimal | Division          |
|:-------:|-----------|---------------|--------:|-------------------|
|    0    | Start     | `0b0001.0000` |      16 | 16 ÷ 1 = 16       |
|    1    | `16 >> 1` | `0b0000.1000` |       8 | 16 ÷ 2 = 8        |
|    2    | `16 >> 2` | `0b0000.0100` |       4 | 16 ÷ 4 = 4        |
|    3    | `16 >> 3` | `0b0000.0010` |       2 | 16 ÷ 8 = 2        |
|    4    | `16 >> 4` | `0b0000.0001` |       1 | 16 ÷ 16 = 1       |
|    5    | `16 >> 5` | `0b0000.0000` |       0 | 16 ÷ 32 = 0,5 → 0 |

### 7.3) Mathematischer Zusammenhang

Rechtsshift um n entspricht ganzzahliger Division durch 2ⁿ (Abrunden):

$$a \gg n = \left\lfloor \frac{a}{2^n} \right\rfloor$$

**Beispiel:** $16 \gg 2 = \lfloor 16 / 4 \rfloor = \lfloor 4 \rfloor = 4$

**Hinweis:** Bits, die rechts „herausfallen", gehen verloren. Das Ergebnis wird abgerundet.

### 7.4) Vergleich: Linksshift vs. Rechtsshift

| Eigenschaft  | Linksshift `<<`         | Rechtsshift `>>`         |
|--------------|-------------------------|--------------------------|
| Symbol       | `<<` (zeigt nach links) | `>>` (zeigt nach rechts) |
| Bit-Bewegung | nach links              | nach rechts              |
| Auffüllung   | rechts mit Nullen       | links mit Nullen         |
| Mathematik   | Multiplikation mit 2ⁿ   | Division durch 2ⁿ        |
| Beispiel     | `1 << 4 = 16`           | `16 >> 2 = 4`            |

> **Merkregel:** Das Symbol zeigt die Richtung der Bit-Bewegung.
> - `<<` Pfeile nach links → Bits wandern nach links → Multiplikation
> - `>>` Pfeile nach rechts → Bits wandern nach rechts → Division

### 7.5) Praxisanwendung: Byte-Index berechnen

Der Compiler optimiert Division durch Zweierpotenzen zu Rechtsshifts:

```c
uint8_t byteIndex = bit / 8;    // Lesbar
uint8_t byteIndex = bit >> 3;   // Identisch, da 8 = 2³
```

Beide Varianten erzeugen denselben Maschinencode. Die erste ist lesbarer, die zweite macht die Bit-Operation explizit.

---

## 8) Bitweise logische Operatoren

**Übersicht:** Vier Operatoren manipulieren einzelne Bits unabhängig voneinander:

| Operator | Name | Sprechweise     | Typische Anwendung       |
|:--------:|------|-----------------|--------------------------|
|   `\|`   | OR   | „Oder"          | Bits setzen              |
|   `&`    | AND  | „Und"           | Bits prüfen / maskieren  |
|   `~`    | NOT  | „Nicht"         | Bits invertieren         |
|   `^`    | XOR  | „Exklusiv-Oder" | Bits umschalten (toggle) |

### 8.1) Wahrheitstabellen

**OR (`|`)** – Ergebnis ist 1, wenn mindestens ein Eingang 1 ist.

| A | B | A \| B |
|:-:|:-:|:------:|
| 0 | 0 |   0    |
| 0 | 1 |   1    |
| 1 | 0 |   1    |
| 1 | 1 |   1    |

*Eselsbrücke:* „Oder" – einer **oder** der andere **oder** beide.

**AND (`&`)** – Ergebnis ist 1, wenn beide Eingänge 1 sind.

| A | B | A & B |
|:-:|:-:|:-----:|
| 0 | 0 |   0   |
| 0 | 1 |   0   |
| 1 | 0 |   0   |
| 1 | 1 |   1   |

*Eselsbrücke:* „Und" – dieser **und** jener müssen beide wahr sein.

**NOT (`~`)** – Invertiert jeden Bit-Wert.

| A | ~A |
|:-:|:--:|
| 0 | 1  |
| 1 | 0  |

*Eselsbrücke:* „Nicht" – das Gegenteil.

**XOR (`^`)** – Ergebnis ist 1, wenn die Eingänge unterschiedlich sind.

| A | B | A ^ B |
|:-:|:-:|:-----:|
| 0 | 0 |   0   |
| 0 | 1 |   1   |
| 1 | 0 |   1   |
| 1 | 1 |   0   |

*Eselsbrücke:* „Entweder-Oder" – aber **nicht** beide gleichzeitig.

---

## 9) Bit-Manipulation: Die vier Grundoperationen

Mit Masken und logischen Operatoren lassen sich einzelne Bits gezielt verändern, ohne andere Bits zu beeinflussen.

### 9.1) Bit setzen mit OR (`|=`)

**Prinzip:** `x | 0 = x` (bleibt) und `x | 1 = 1` (wird gesetzt)

**Code-Muster:**

```c
ledState |= (1u << bitPos);   // Bit an Position bitPos setzen
```

**Anwendungsbeispiel: Bit 4 setzen**

```
Ausgangszustand:  ledState = 0b1000.0000  (Bit 7 bereits gesetzt)
Maske:                       0b0001.0000  (1u << 4)
                             ───────────  OR
Ergebnis:                    0b1001.0000  (Bit 7 bleibt, Bit 4 neu)

Bit-für-Bit-Analyse:
Position:   7   6   5   4   3   2   1   0
ledState:   1   0   0   0   0   0   0   0
Maske:      0   0   0   1   0   0   0   0
            ─   ─   ─   ─   ─   ─   ─   ─  OR
Ergebnis:   1   0   0   1   0   0   0   0
            │           │
            │           └── 0|1 = 1 (gesetzt)
            └── 1|0 = 1 (unverändert)
```

### 9.2) Bit löschen mit AND NOT (`&= ~`)

**Prinzip:** `x & 1 = x` (bleibt) und `x & 0 = 0` (wird gelöscht)

**Code-Muster:**

```c
ledState &= ~(1u << bitPos);   // Bit an Position bitPos löschen
```

**Anwendungsbeispiel: Bit 4 löschen**

```
Schritt 1: Maske erzeugen
    1u << 4 = 0b0001.0000

Schritt 2: Maske invertieren
    ~0b0001.0000 = 0b1110.1111

Schritt 3: AND anwenden
    Ausgangszustand:  ledState = 0b1001.0011  (mehrere Bits gesetzt)
    Invertierte Maske:           0b1110.1111
                                 ───────────  AND
    Ergebnis:                    0b1000.0011  (nur Bit 4 gelöscht)

Bit-für-Bit-Analyse:
Position:   7   6   5   4   3   2   1   0
ledState:   1   0   0   1   0   0   1   1
~Maske:     1   1   1   0   1   1   1   1
            ─   ─   ─   ─   ─   ─   ─   ─  AND
Ergebnis:   1   0   0   0   0   0   1   1
                        │
                        └── 1&0 = 0 (gelöscht)
```

### 9.3) Bit umschalten mit XOR (`^=`)

**Prinzip:** `x ^ 0 = x` (bleibt) und `x ^ 1 = ~x` (wird invertiert)

**Code-Muster:**

```c
ledState ^= (1u << bitPos);   // Bit an Position bitPos toggeln
```

**Anwendungsbeispiel: Bit 4 toggeln**

```
Fall 1: Bit ist 0 → wird 1
    ledState:   0b0000.0000
    Maske:      0b0001.0000
                ───────────  XOR
    Ergebnis:   0b0001.0000   (0 ^ 1 = 1)

Fall 2: Bit ist 1 → wird 0
    ledState:   0b0001.0000
    Maske:      0b0001.0000
                ───────────  XOR
    Ergebnis:   0b0000.0000   (1 ^ 1 = 0)
```

*Anwendung:* Blinken ohne Zustandsvariable – jeder Aufruf wechselt den Zustand.

### 9.4) Bit prüfen mit AND (`&`)

**Prinzip:** AND mit Maske isoliert das gewünschte Bit. Ergebnis ≠ 0 bedeutet „gesetzt".

**Code-Muster:**

```c
if (ledState & (1u << bitPos)) {
    // Bit ist gesetzt (= 1)
} else {
    // Bit ist nicht gesetzt (= 0)
}
```

**Anwendungsbeispiel: Bit 4 prüfen**

```
Fall 1: Bit 4 ist gesetzt
    ledState:   0b0001.0000
    Maske:      0b0001.0000
                ───────────  AND
    Ergebnis:   0b0001.0000 = 16 ≠ 0 → true

Fall 2: Bit 4 ist nicht gesetzt
    ledState:   0b0000.0000
    Maske:      0b0001.0000
                ───────────  AND
    Ergebnis:   0b0000.0000 = 0 → false
```

---

## 10) Zusammenfassung

### 10.1) Die vier Bit-Operationen

| Operation      | Code                 | Wirkung            | Wahrheitstabelle   |
|----------------|----------------------|--------------------|--------------------|
| Bit setzen     | `x \|= (1u << n);`   | 0→1, 1→1           | `x \| 1 = 1`       |
| Bit löschen    | `x &= ~(1u << n);`   | 1→0, 0→0           | `x & 0 = 0`        |
| Bit umschalten | `x ^= (1u << n);`    | 0→1, 1→0           | `x ^ 1 = ~x`       |
| Bit prüfen     | `if (x & (1u << n))` | Ergebnis ≠0 wenn 1 | Isolation des Bits |

### 10.2) Adressierungsformeln (1-basierte IDs)

| Berechnung   | Formel              | Beispiel (ID=87) |
|--------------|---------------------|------------------|
| Byte-Anzahl  | `(count + 7) / 8`   | (100+7)/8 = 13   |
| Byte-Index   | `(id - 1) / 8`      | (87−1)/8 = 10    |
| Bit-Position | `(id - 1) % 8`      | (87−1)%8 = 6     |
| Maske        | `1u << bitPosition` | 1u << 6 = 0x40   |

### 10.3) Hex-Masken-Referenz

| Bit | Dezimal |  Hex   |   | Bit | Dezimal |  Hex   |
|:---:|--------:|:------:|---|:---:|--------:|:------:|
|  0  |       1 | `0x01` |   |  4  |      16 | `0x10` |
|  1  |       2 | `0x02` |   |  5  |      32 | `0x20` |
|  2  |       4 | `0x04` |   |  6  |      64 | `0x40` |
|  3  |       8 | `0x08` |   |  7  |     128 | `0x80` |

> **Merkregel:** Low-Nibble: 1, 2, 4, 8 → High-Nibble: 10, 20, 40, 80 (in Hex)

---

## 11) Vollständiges Anwendungsbeispiel

### 11.1) Aufgabe: LED 5 einschalten

```c
// Ziel: LED 5 einschalten
uint8_t selected_id = 5;

// Schritt 1: Nullbasierter Index
uint8_t bit = selected_id - 1;        // bit = 4

// Schritt 2: Byte-Index
uint8_t byteIndex = bit / 8;          // 4 / 8 = 0

// Schritt 3: Bit-Position
uint8_t bitPosition = bit % 8;        // 4 % 8 = 4

// Schritt 4: Maske
uint8_t mask = 1u << bitPosition;     // 1 << 4 = 0x10

// Schritt 5: Bit setzen
ledState[byteIndex] |= mask;
```

### 11.2) Visualisierung

```
LED-Nummern:      1    2    3    4    5    6    7    8
                  │    │    │    │    │    │    │    │
Bit-Positionen:   0    1    2    3    4    5    6    7
                                      ▲
                                      └── LED 5 = Bit 4

Byte vorher:     0b0000.0000
Maske (0x10):    0b0001.0000
                 ───────────  OR
Byte nachher:    0b0001.0000    ← LED 5 ist an
```

### 11.3) Komplette Umrechnungstabelle (LEDs 1–10)

| LED | bit | Byte | Bit-Pos | Maske (binär) | Maske (hex) |
|----:|----:|-----:|--------:|---------------|:-----------:|
|   1 |   0 |    0 |       0 | `0b0000.0001` |   `0x01`    |
|   2 |   1 |    0 |       1 | `0b0000.0010` |   `0x02`    |
|   3 |   2 |    0 |       2 | `0b0000.0100` |   `0x04`    |
|   4 |   3 |    0 |       3 | `0b0000.1000` |   `0x08`    |
|   5 |   4 |    0 |       4 | `0b0001.0000` |   `0x10`    |
|   6 |   5 |    0 |       5 | `0b0010.0000` |   `0x20`    |
|   7 |   6 |    0 |       6 | `0b0100.0000` |   `0x40`    |
|   8 |   7 |    0 |       7 | `0b1000.0000` |   `0x80`    |
|   9 |   8 |    1 |       0 | `0b0000.0001` |   `0x01`    |
|  10 |   9 |    1 |       1 | `0b0000.0010` |   `0x02`    |

