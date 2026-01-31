# Embedded-Grundlagen: Systematische Einführung


## 0) Begriffsrahmen und Zielsetzung

### Anwendungskontext

Embedded-Systeme unterscheiden sich fundamental von Desktop-Software. Die „API" ist physisch – Pins, Spannungspegel, Timing. Fehler führen nicht zu Exceptions, sondern zu falschen Signalen, zerstörter Hardware oder unerklärlichem Verhalten. Diese Dokumentation vermittelt die physikalischen und konzeptionellen Grundlagen, die hinter dem Selection-Panel-Code stehen.

### Lernziele

Nach dem Durcharbeiten dieses Dokuments kannst du:

1. **Das Schichtenmodell erklären** – verstehen, wie Software über HAL und Register mit Hardware kommuniziert
2. **Spannungspegel interpretieren** – HIGH, LOW und undefinierte Bereiche unterscheiden
3. **Active-Low/Active-High anwenden** – die Logik von Tastern und LEDs korrekt implementieren
4. **Schieberegister einsetzen** – CD4021 für Eingabe und 74HC595 für Ausgabe verdrahten und ansteuern
5. **SPI-Parameter konfigurieren** – Modi, Frequenzen und Timing-Anforderungen einhalten
6. **Timing-Budgets berechnen** – Scan-Intervalle einhalten und Determinismus sicherstellen
7. **Debouncing implementieren** – Prellvorgänge filtern und saubere Flanken erkennen
8. **One-Hot-Encoding garantieren** – die Invariante durch Konstruktion sicherstellen

### Begriffsdefinitionen

| Begriff             | Definition                                                                | Quelle/Norm           |
|---------------------|---------------------------------------------------------------------------|-----------------------|
| **GPIO**            | General Purpose Input/Output – frei konfigurierbarer Mikrocontroller-Pin  | Embedded-Terminologie |
| **HAL**             | Hardware Abstraction Layer – Schicht zwischen Anwendung und Registern     | CMSIS, AUTOSAR        |
| **Active-Low**      | Signal ist aktiv bei Pegel 0 (GND), inaktiv bei 1 (VCC)                   | Digital-Design        |
| **Active-High**     | Signal ist aktiv bei Pegel 1 (VCC), inaktiv bei 0 (GND)                   | Digital-Design        |
| **Debouncing**      | Entprellung – Filterung mechanischer Prellvorgänge durch Zeit-Schwellwert | Embedded-Praxis       |
| **SPI**             | Serial Peripheral Interface – synchroner Vollduplex-Datenbus              | Motorola, de-facto    |
| **Schieberegister** | IC zur Konvertierung zwischen parallelen und seriellen Daten              | Digital-Design        |
| **Pullup**          | Widerstand, der einen Pin auf HIGH zieht, wenn nicht aktiv getrieben      | Schaltungstechnik     |
| **Pulldown**        | Widerstand, der einen Pin auf LOW zieht, wenn nicht aktiv getrieben       | Schaltungstechnik     |
| **CPOL**            | Clock Polarity – Ruhezustand des SPI-Takts (0=LOW, 1=HIGH)                | SPI-Spezifikation     |
| **CPHA**            | Clock Phase – bei welcher Flanke werden Daten gesampelt                   | SPI-Spezifikation     |
| **One-Hot**         | Codierung: höchstens ein Bit gesetzt, alle anderen 0                      | Digital-Design        |
| **SSOT**            | Single Source of Truth – jede Konfiguration an genau einer Stelle         | Design-Patterns       |

---

## 1) Hardware ist die Schnittstelle

### Problemstellung

In Desktop-Software rufst du Funktionen auf und bekommst Rückgabewerte. Der Compiler garantiert Typsicherheit, das Betriebssystem schützt vor Speicherzugriffsfehlern. In Embedded-Systemen kommunizierst du direkt mit der Außenwelt über elektrische Signale – ohne Sicherheitsnetz.

### Herleitung: Die Schichten eines Embedded-Systems

```
┌─────────────────────────────────────────────────────────────┐
│  Anwendungslogik (Selection, Debounce, State-Machine)       │  ← Dein Code
├─────────────────────────────────────────────────────────────┤
│  Treiber (CD4021, 74HC595)                                  │  ← IC-spezifisch
├─────────────────────────────────────────────────────────────┤
│  HAL (SPI-Transfer, GPIO-Zugriff)                           │  ← Plattform-Abstraktion
├─────────────────────────────────────────────────────────────┤
│  Register (ESP32-S3 Peripherie)                             │  ← Hardware-Adressen
├─────────────────────────────────────────────────────────────┤
│  Physische Pins (3,3 V / GND / Signale)                     │  ← Elektrische Realität
└─────────────────────────────────────────────────────────────┘
```

### Anwendung: Fehlerquellen auf jeder Ebene

| Ebene           | Typischer Fehler                     | Symptom                   |
|-----------------|--------------------------------------|---------------------------|
| Anwendungslogik | Falsche Bit-Maske                    | Falscher Button erkannt   |
| Treiber         | Falsches Timing für Latch-Puls       | LEDs flackern             |
| HAL             | SPI-Modus 0 statt 1                  | Daten um 1 Bit verschoben |
| Register        | Falsches Peripherie-Clock-Enable     | SPI sendet nichts         |
| Physische Pins  | Pins vertauscht oder nicht verbunden | Komplett falsche Signale  |

### Regel

Ein Fehler auf jeder Ebene führt zu Fehlfunktion. Wenn der SPI-Modus falsch ist, liefert der CD4021 Müll – auch wenn dein Code syntaktisch korrekt ist. **Debugging muss von unten nach oben erfolgen:** Erst Verkabelung, dann Register, dann HAL, dann Treiber, dann Logik.

### Zusammenfassung

> **Merkregel:** „Software-Bugs crashen, Hardware-Bugs schweigen. Prüfe von unten nach oben."

---

## 2) Spannungspegel – Die Sprache der Hardware

### Problemstellung

Digitale Signale in Software sind abstrakt: 0 oder 1, true oder false. In Hardware sind das Spannungsbereiche mit physikalischen Grenzen und verbotenen Zonen.

### Herleitung: Logikpegel bei 3,3 V

| Zustand     | Spannungsbereich | Bedeutung                      |
|-------------|------------------|--------------------------------|
| LOW (0)     | 0 V – 0,8 V      | Logisch 0, definiert           |
| Undefiniert | 0,8 V – 2,0 V    | **Verbotene Zone!** Metastabil |
| HIGH (1)    | 2,0 V – 3,3 V    | Logisch 1, definiert           |

**Visualisierung:**

```
3,3 V ─────┬───────────────────── HIGH (logisch 1)
           │
2,0 V ─────┼─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ Grenze HIGH
           │
           │     VERBOTEN!         Undefiniert (metastabil)
           │
0,8 V ─────┼─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ Grenze LOW
           │
0 V   ─────┴───────────────────── LOW (logisch 0)
```

**Warum ist die undefinierte Zone gefährlich?**

In diesem Bereich „weiß" der Eingang nicht, ob er 0 oder 1 sehen soll. Das Ergebnis kann zwischen 0 und 1 oszillieren oder bei jedem Lesen unterschiedlich sein. Der Zustand ist metastabil – wie eine Münze, die auf der Kante balanciert.

### Anwendung: Pullup- und Pulldown-Widerstände

**Problem:** Ein offener Pin (nicht verbunden) hat keinen definierten Pegel – er „schwebt" und nimmt zufällige Werte durch Störeinkopplung an.

**Lösung:** Ein Pullup-Widerstand zieht den Pin auf HIGH, solange nichts anderes ihn aktiv auf LOW treibt.

**Schaltbild – Taster mit Pullup (Active-Low):**

```
VCC (3,3 V)
    │
   ┌┴┐
   │ │  R = 10 kΩ (Pullup)
   └┬┘
    ├──────────────────► zum GPIO-Pin
    │
   ╱    Taster (mechanisch)
    │
   GND

Taster offen:   Pin = HIGH (3,3 V durch Pullup)
Taster gedrückt: Pin = LOW  (GND überstimmt Pullup)
```

**Warum 10 kΩ?**

- Zu niedrig (100 Ω): Hoher Stromverbrauch bei gedrücktem Taster
- Zu hoch (1 MΩ): Zu langsam für schnelle Signale, anfällig für Störungen
- 10 kΩ ist ein Kompromiss: niedrige Stromaufnahme (~0,33 mA), schnelle Anstiegszeit

**Berechnung des Stroms bei gedrücktem Taster:**

$$I = \frac{U}{R} = \frac{3{,}3\,\text{V}}{10\,\text{k}\Omega} = 0{,}33\,\text{mA}$$

Bei 100 Tastern mit gleichzeitigem Drücken: $100 \times 0{,}33\,\text{mA} = 33\,\text{mA}$ – akzeptabel.

### Zusammenfassung

> **Merkregel:** „Floating Pins sind der Feind. Pullup für HIGH-Default, Pulldown für LOW-Default."

---

## 3) Active-Low vs. Active-High

### Problemstellung

Warum ist ein gedrückter Taster „0" und eine leuchtende LED „1"? Das scheint unintuitiv – müsste „aktiv" nicht immer „1" sein?

### Herleitung: Die physikalische Begründung

| Typ         | Aktiv bei | Inaktiv bei | Typischer Einsatz                |
|-------------|-----------|-------------|----------------------------------|
| Active-Low  | 0 (GND)   | 1 (VCC)     | Taster mit Pullup, Reset-Signale |
| Active-High | 1 (VCC)   | 0 (GND)     | LEDs, Enable-Signale             |

**Warum Active-Low für Taster?**

Mit Pullup ist der Ruhezustand definiert (HIGH). Der Taster muss nur nach GND schalten – das ist:
1. **Schaltungstechnisch einfacher:** Taster hat nur zwei Kontakte
2. **Robuster gegen Störungen:** GND ist eine „härtere" Referenz als VCC
3. **Historisch etabliert:** TTL-Logik hatte stärkere LOW-Treiber

**Warum Active-High für LEDs?**

LEDs leuchten, wenn Strom fließt – von der Anode (+) zur Kathode (–).

**Schaltbild – LED Active-High:**

```
GPIO-Pin ────┬──── 330 Ω ────┬──── LED ────┬──── GND
             │               │             │
           3,3 V           ~3 V          ~2 V          0 V
             └───────────── Spannungsabfall ──────────┘

Pin = HIGH: 3,3 V → Strom fließt → LED leuchtet
Pin = LOW:  0 V   → kein Strom   → LED aus
```

**Berechnung des Vorwiderstands:**

$$R = \frac{U_{\text{GPIO}} - U_{\text{LED}}}{I_{\text{LED}}} = \frac{3{,}3\,\text{V} - 2{,}0\,\text{V}}{5\,\text{mA}} = 260\,\Omega \approx 330\,\Omega$$

(Standard-Wert 330 Ω ergibt ca. 4 mA – ausreichend für sichtbares Leuchten)

### Anwendung im Code

```c
// Button lesen (Active-Low: gedrückt = 0)
uint8_t raw = readShiftRegister();
bool button1Pressed = (raw & 0x80) == 0;  // Bit 7 = 0 → gedrückt

// LED setzen (Active-High: an = 1)
uint8_t ledState = 0x00;
if (led1On) {
    ledState |= 0x80;   // Bit 7 = 1 → LED 1 an
}
if (led2On) {
    ledState |= 0x40;   // Bit 6 = 1 → LED 2 an
}
writeShiftRegister(ledState);
```

**Bit-Operationen erklärt:**

| Operation           | Wirkung                         | Beispiel                   |
|---------------------|---------------------------------|----------------------------|
| `raw & 0x80`        | Isoliert Bit 7                  | `0b10101010 & 0x80 = 0x80` |
| `(raw & 0x80) == 0` | Prüft ob Bit 7 = 0 (Active-Low) | `0x80 == 0` → `false`      |
| `state |= 0x80`     | Setzt Bit 7 auf 1               | `0x00 |= 0x80 = 0x80`      |
| `state &= ~0x80`    | Löscht Bit 7 (setzt auf 0)      | `0xFF &= 0x7F = 0x7F`      |

### Zusammenfassung

> **Merkregel:** „Taster mit Pullup = Active-Low. LED nach GND = Active-High. Die Physik bestimmt die Logik."

---

## 4) Schieberegister – Pins multiplizieren

### Problemstellung

Der ESP32-S3 hat begrenzte GPIO-Pins. Für 100 Buttons und 100 LEDs bräuchtest du 200 Pins – die gibt es nicht. Selbst wenn, wäre die Verkabelung unmöglich.

### Regel

Schieberegister wandeln zwischen parallelen und seriellen Daten um. Mit wenigen seriellen Pins steuerst du viele parallele Signale.

### Herleitung: CD4021B – Parallel zu Seriell (Buttons lesen)

**Funktionsprinzip:**

```
        D7  D6  D5  D4  D3  D2  D1  D0    ← 8 Buttons (parallel)
         │   │   │   │   │   │   │   │
         ▼   ▼   ▼   ▼   ▼   ▼   ▼   ▼
        ┌───────────────────────────────┐
        │         CD4021B               │
        │    8-Bit PISO Shift Register  │
        │                               │
        │  PS/LOAD ───► Latch Control   │
        │  CLK     ───► Shift Clock     │
        │  Q7      ───► Serial Out      │
        └───────────────────────────────┘
                      │
                      ▼
                 Serial Out ──────────► ESP32 MISO (1 Pin)
```

**PISO = Parallel-In, Serial-Out**

**Timing-Ablauf:**

```
        ┌─────────────────────────────────────────────────────────────┐
PS/LOAD │████│                                                        │
        └────┘
             │
             │ Parallele Eingänge werden ins Register übernommen
             ▼
CLK     ────────┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──────
                └──┘  └──┘  └──┘  └──┘  └──┘  └──┘  └──┘  └──┘
                  1     2     3     4     5     6     7     8
                  │     │     │     │     │     │     │     │
Q7 (Data)       D7    D6    D5    D4    D3    D2    D1    D0
                  │     │     │     │     │     │     │     │
                  ▼     ▼     ▼     ▼     ▼     ▼     ▼     ▼
                Bit 7 Bit 6 Bit 5 Bit 4 Bit 3 Bit 2 Bit 1 Bit 0
```

**Timing-Parameter (aus Datenblatt):**

| Parameter          | Symbol | Minimum | Typisch | Einheit |
|--------------------|--------|---------|---------|---------|
| PS/LOAD Pulsbreite | t_PW   | 180     | –       | ns      |
| Clock Frequenz     | f_CLK  | –       | 5       | MHz     |
| Setup Time         | t_SU   | 50      | –       | ns      |
| Hold Time          | t_H    | 50      | –       | ns      |

Bei 500 kHz SPI-Frequenz (2 µs Periode) sind wir weit im sicheren Bereich.

### Herleitung: 74HC595 – Seriell zu Parallel (LEDs schreiben)

**Funktionsprinzip:**

```
ESP32 MOSI (1 Pin) ──────────► Serial In
                                   │
                                   ▼
        ┌───────────────────────────────┐
        │         74HC595               │
        │    8-Bit SIPO Shift Register  │
        │                               │
        │  SRCLK  ───► Shift Clock      │
        │  RCLK   ───► Latch Clock      │
        │  OE     ───► Output Enable    │
        └───────────────────────────────┘
         │   │   │   │   │   │   │   │
         ▼   ▼   ▼   ▼   ▼   ▼   ▼   ▼
        Q7  Q6  Q5  Q4  Q3  Q2  Q1  Q0    ← 8 LEDs (parallel)
```

**SIPO = Serial-In, Parallel-Out**

**Timing-Ablauf:**

```
Data (MOSI)  ──D7──D6──D5──D4──D3──D2──D1──D0────────────────────
                │    │    │    │    │    │    │    │
SRCLK        ───┴────┴────┴────┴────┴────┴────┴────┴─────────────
                      (8 Shift-Pulse)
                                                    │
RCLK (Latch) ───────────────────────────────────────┴───┐  ┌─────
                                                        └──┘
                                                     Latch-Puls
Outputs      ──────────── alte Werte ──────────────────┬── neue ─
                                                       │
                                        Daten werden übernommen
```

**Warum der Latch?**

Ohne Latch würden die Ausgänge während des Schiebens durchlaufen – die LEDs würden für Mikrosekunden falsche Muster zeigen (Flackern). Der Latch hält die alten Werte stabil, bis alle 8 neuen Bits eingeschoben sind.

### Anwendung: Kaskadierung für mehr Kanäle

Mehrere Schieberegister werden in Reihe geschaltet:

```
CD4021 Kaskade (Buttons lesen):
───────────────────────────────────────────────────────────────────
Button 1-8      Button 9-16     Button 17-24           Button 97-100
    │               │               │                      │
    ▼               ▼               ▼                      ▼
┌────────┐     ┌────────┐     ┌────────┐              ┌────────┐
│CD4021  │────►│CD4021  │────►│CD4021  │── ... ──►   │CD4021  │──► MISO
│ IC #0  │ Q7  │ IC #1  │ Q7  │ IC #2  │              │ IC #12 │
└────────┘     └────────┘     └────────┘              └────────┘
                                                           │
                                                        ESP32

74HC595 Kaskade (LEDs schreiben):
───────────────────────────────────────────────────────────────────
                                                        ESP32
                                                           │
                                                        MOSI
                                                           │
┌────────┐     ┌────────┐     ┌────────┐              ┌────────┐
│74HC595 │◄────│74HC595 │◄────│74HC595 │◄── ... ◄────│74HC595 │
│ IC #0  │ SER │ IC #1  │ SER │ IC #2  │              │ IC #12 │
└────────┘     └────────┘     └────────┘              └────────┘
    │               │               │                      │
    ▼               ▼               ▼                      ▼
LED 1-8         LED 9-16        LED 17-24              LED 97-100
```

**Pin-Einsparung:**

| Kanäle | Direkte GPIOs | Mit Schieberegistern     | Einsparung |
|--------|---------------|--------------------------|------------|
| 8      | 8             | 4 (SCK, MISO, PS, LATCH) | 50%        |
| 16     | 16            | 4                        | 75%        |
| 100    | 100           | 5 (+ MOSI)               | 95%        |

### Zusammenfassung

> **Merkregel:** „CD4021 = Einlesen (Parallel→Seriell), 74HC595 = Ausgeben (Seriell→Parallel). Kaskadieren = Daisy-Chain."

---

## 5) SPI – Der Datenbus

### Problemstellung

Wie überträgst du Daten zwischen ESP32 und Schieberegistern synchron und zuverlässig? Die Chips müssen wissen, wann ein Bit gültig ist.

### Regel

SPI (Serial Peripheral Interface) ist ein synchroner Bus – ein gemeinsamer Takt (SCK) synchronisiert alle Übertragungen.

### Herleitung: SPI-Signale

| Signal | Name                | Richtung       | Funktion                                |
|--------|---------------------|----------------|-----------------------------------------|
| SCK    | Serial Clock        | Master → Slave | Taktsignal (synchronisiert Übertragung) |
| MOSI   | Master Out Slave In | Master → Slave | Daten vom Master zum Slave              |
| MISO   | Master In Slave Out | Slave → Master | Daten vom Slave zum Master              |
| CS/SS  | Chip Select         | Master → Slave | Welcher Slave ist aktiv (Active-Low)    |

**Timing-Diagramm (Vollduplex):**

```
SCK     ────┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌─────
            └──┘  └──┘  └──┘  └──┘  └──┘  └──┘  └──┘  └──┘
               ↑     ↑     ↑     ↑     ↑     ↑     ↑     ↑
            Sample-Zeitpunkte (abhängig von Mode)

MOSI    ──┬─D7─┬─D6─┬─D5─┬─D4─┬─D3─┬─D2─┬─D1─┬─D0─┬──────────
          └────┴────┴────┴────┴────┴────┴────┴────┘
              Master sendet zu Slave

MISO    ──┬─D7─┬─D6─┬─D5─┬─D4─┬─D3─┬─D2─┬─D1─┬─D0─┬──────────
          └────┴────┴────┴────┴────┴────┴────┴────┘
              Slave sendet zu Master (gleichzeitig!)
```

### Herleitung: SPI-Modi (CPOL und CPHA)

**Problem:** Wann genau wird ein Bit gelesen – bei steigender oder fallender Flanke? Das hängt von zwei Parametern ab.

| Parameter | Bedeutung                                       |
|-----------|-------------------------------------------------|
| CPOL      | Clock Polarity – Ruhezustand des Takts          |
| CPHA      | Clock Phase – bei welcher Flanke wird gesampelt |

**Die vier Modi:**

| Mode | CPOL | CPHA | Clock Idle | Sample bei     | Shift bei      |
|:----:|:----:|:----:|------------|----------------|----------------|
|  0   |  0   |  0   | LOW        | Rising Edge ↑  | Falling Edge ↓ |
|  1   |  0   |  1   | LOW        | Falling Edge ↓ | Rising Edge ↑  |
|  2   |  1   |  0   | HIGH       | Falling Edge ↓ | Rising Edge ↑  |
|  3   |  1   |  1   | HIGH       | Rising Edge ↑  | Falling Edge ↓ |

**Visualisierung Mode 0 vs. Mode 1:**

```
Mode 0 (CPOL=0, CPHA=0):
────────────────────────────────────────────────────────────
SCK     ────┐  ┌──┐  ┌──┐  ┌──────
            └──┘  └──┘  └──┘
               ↑     ↑     ↑
            Sample (Rising Edge)

Mode 1 (CPOL=0, CPHA=1):
────────────────────────────────────────────────────────────
SCK     ────┐  ┌──┐  ┌──┐  ┌──────
            └──┘  └──┘  └──┘
            ↓     ↓     ↓
         Sample (Falling Edge)
```

### Anwendung im Projekt

| IC      | Mode | Frequenz | Begründung                     |
|---------|:----:|----------|--------------------------------|
| CD4021B |  1   | 500 kHz  | Datenblatt empfiehlt Mode 1/2  |
| 74HC595 |  0   | 1 MHz    | Standard-Mode, weit verbreitet |

**ESP32-S3 Konfiguration:**

```c
// CD4021 (Button-Input)
spi_device_interface_config_t cd4021_cfg = {
    .clock_speed_hz = 500000,    // 500 kHz
    .mode = 1,                   // CPOL=0, CPHA=1
    .spics_io_num = -1,          // Manuelles CS (PS/LOAD)
    .queue_size = 1
};

// 74HC595 (LED-Output)
spi_device_interface_config_t hc595_cfg = {
    .clock_speed_hz = 1000000,   // 1 MHz
    .mode = 0,                   // CPOL=0, CPHA=0
    .spics_io_num = -1,          // Manuelles CS (LATCH)
    .queue_size = 1
};
```

**Konsequenz:** Wenn du den falschen Modus verwendest, liest du Müll oder die Daten sind um ein Bit verschoben. Das äußert sich oft als „Button 1 triggert Button 2".

### Zusammenfassung

> **Merkregel:** „Mode im Datenblatt nachschlagen, nie raten. Ein falsches Mode-Bit = alles verschoben."

---

## 6) Timing – Determinismus ist Pflicht

### Problemstellung

Embedded-Software muss in definierten Zeitfenstern reagieren. Wenn du eine Deadline verpasst, funktioniert das System nicht – Buttons reagieren nicht, LEDs flackern, Kommunikation bricht ab.

### Herleitung: Timing-Budget

**Scan-Intervall:** 10 ms (100 Hz) – schnell genug für Benutzerinteraktion, langsam genug für Debouncing.

```
┌──────────────────────────────────────────────────────────────┐
│  10 ms Scan-Intervall (10.000 µs)                            │
│                                                              │
│  ├── CD4021 read ──┤  20 µs                                  │
│  ├── Debounce ─────┤   5 µs                                  │
│  ├── Logic ────────┤   5 µs                                  │
│  ├── HC595 write ──┤  20 µs                                  │
│  ├─────────────────────────────────── Reserve ──────────────┤│
│                                                  9.950 µs    │
└──────────────────────────────────────────────────────────────┘
```

**Timing-Budget Berechnung:**

| Operation             | Dauer     | Anteil    |
|-----------------------|-----------|-----------|
| CD4021 read (2 Bytes) | ~20 µs    | 0,2%      |
| Debounce (10 Buttons) | ~5 µs     | 0,05%     |
| Selection-Logik       | ~5 µs     | 0,05%     |
| HC595 write (2 Bytes) | ~20 µs    | 0,2%      |
| **Summe**             | ~50 µs    | **0,5%**  |
| **Reserve**           | ~9.950 µs | **99,5%** |

Bei 100 Kanälen (13 Bytes):

| Operation              | Dauer     | Anteil    |
|------------------------|-----------|-----------|
| CD4021 read (13 Bytes) | ~130 µs   | 1,3%      |
| Debounce (100 Buttons) | ~50 µs    | 0,5%      |
| Selection-Logik        | ~10 µs    | 0,1%      |
| HC595 write (13 Bytes) | ~130 µs   | 1,3%      |
| **Summe**              | ~320 µs   | **3,2%**  |
| **Reserve**            | ~9.680 µs | **96,8%** |

### Anwendung: Was zerstört Timing?

| Gut ✓                          | Schlecht ✗                |
|--------------------------------|---------------------------|
| `vTaskDelayUntil()` (FreeRTOS) | `delay()` im Hot-Path     |
| Logging bei Zustandsänderung   | Logging pro Scan-Zyklus   |
| Kurze SPI-Transfers            | Blockierende UART-Ausgabe |
| Interrupt-basierte Ereignisse  | Polling in Busy-Loop      |

### Herleitung: UART-Overhead

**Berechnung:**

Bei 115200 Baud dauert ein Zeichen:

$$t_{\text{char}} = \frac{10 \text{ Bits}}{115200 \text{ Baud}} = 86{,}8\,\mu\text{s} \approx 87\,\mu\text{s}$$

(10 Bits = 1 Start + 8 Daten + 1 Stop)

Eine Debug-Zeile mit 50 Zeichen:

$$t_{\text{line}} = 50 \times 87\,\mu\text{s} = 4{,}35\,\text{ms}$$

Das sind **43,5% des 10-ms-Intervalls** – für eine einzige Debug-Zeile!

**Konsequenz:** Debug-Ausgaben nur bei Events (Zustandsänderungen), nie pro Scan-Zyklus.

```c
// ✗ SCHLECHT: Logging pro Zyklus
void scanButtons() {
    uint8_t raw = readCD4021();
    Serial.printf("RAW: %02X\n", raw);  // 4+ ms blockiert!
    processButtons(raw);
}

// ✓ GUT: Logging nur bei Änderung
void scanButtons() {
    uint8_t raw = readCD4021();
    if (raw != lastRaw) {
        Serial.printf("RAW changed: %02X\n", raw);  // Selten
        lastRaw = raw;
    }
    processButtons(raw);
}
```

### Zusammenfassung

> **Merkregel:** „Im Hot-Path kein printf(). Messe Timing, rechne vorher, teste danach."

---

## 7) Debouncing – Mechanisches Prellen filtern

### Problemstellung

Mechanische Taster prellen. Ein einzelner Druck erzeugt mehrere Flanken in wenigen Millisekunden – ein Mikrokontroller sieht statt einem Button-Druck 3–20 Einzelevents.

**Visualisierung:**

```
Ideal (erwartet):
────────────┐                    ┌────────────────────
            │                    │
            └────────────────────┘
              ◄── gedrückt ────►

Real (gemessen):
────────────┐ ┌┐ ┌┐              ┌┐ ┌┐┌───────────────
            │ ││ ││              ││ │││
            └─┘└─┘└──────────────┘└─┘└┘
              ◄──── Prellen ────►  ◄─ Prellen beim Loslassen
```

### Herleitung: Prellverhalten

**Messwerte (typisch für taktile Mikrotaster):**

| Parameter            | Minimum | Typisch | Maximum | Einheit |
|----------------------|---------|---------|---------|---------|
| Prellzeit            | 5       | 20      | 50      | ms      |
| Anzahl Prellvorgänge | 3       | 10      | 20      | –       |
| Prellperiode         | 0,5     | 2       | 5       | ms      |

**Konsequenz ohne Debouncing:**

```
User drückt Button 1 einmal
→ Firmware sieht: Press, Release, Press, Release, Press...
→ Selection toggelt 10× in 50 ms
→ Endresultat: zufällig an oder aus
```

### Anwendung: Software-Debounce-Algorithmus

**Prinzip:** Ein neuer Zustand wird erst akzeptiert, wenn er für N aufeinanderfolgende Samples stabil bleibt.

```c
typedef struct {
    uint8_t stable;     // Letzter stabiler Zustand (0 oder 1)
    uint8_t counter;    // Zähler für aufeinanderfolgende gleiche Samples
} DebounceState;

/**
 * Aktualisiert den Debounce-Zustand und erkennt Press-Edges.
 *
 * @param state     Zeiger auf Debounce-Zustand
 * @param current   Aktueller Rohwert (0 oder 1)
 * @param threshold Anzahl stabiler Samples für Zustandswechsel
 * @return true wenn Press-Edge erkannt (war HIGH, ist jetzt stabil LOW)
 */
bool debounce_update(DebounceState* state, uint8_t current, uint8_t threshold) {
    // Fall 1: Rohwert == stabiler Zustand → bestätigt, Reset
    if (current == state->stable) {
        state->counter = 0;
        return false;
    }

    // Fall 2: Rohwert != stabiler Zustand → Zähler erhöhen
    state->counter++;

    // Fall 3: Threshold erreicht → neuer stabiler Zustand
    if (state->counter >= threshold) {
        bool wasHigh = (state->stable != 0);
        bool isLow   = (current == 0);

        state->stable = current;
        state->counter = 0;

        // Press-Edge bei Active-Low: war HIGH, ist jetzt LOW
        return (wasHigh && isLow);
    }

    return false;  // Noch nicht stabil genug
}
```

### Herleitung: Threshold-Berechnung

**Formel:**

$$\text{threshold} = \left\lceil \frac{t_{\text{debounce}}}{t_{\text{scan}}} \right\rceil$$

**Beispiel:**

| Gewünschte Debounce-Zeit | Scan-Intervall | Threshold |
|--------------------------|----------------|-----------|
| 30 ms                    | 10 ms          | 3         |
| 50 ms                    | 10 ms          | 5         |
| 50 ms                    | 5 ms           | 10        |

Bei 10 ms Scan-Intervall und 50 ms gewünschter Debounce-Zeit:

$$\text{threshold} = \frac{50\,\text{ms}}{10\,\text{ms}} = 5$$

### Anwendung: Zustandsdiagramm

```
                    ┌─────────────────────────────────┐
                    │                                 │
                    ▼                                 │
              ┌──────────┐                            │
     ┌───────►│  STABLE  │◄───────┐                   │
     │        │ (HIGH)   │        │                   │
     │        └────┬─────┘        │                   │
     │             │              │                   │
     │     current == 0           │ current == 1      │
     │             │              │ (counter < thres) │
     │             ▼              │                   │
     │        ┌──────────┐        │                   │
     │        │ COUNTING │────────┘                   │
     │        │ (to LOW) │                            │
     │        └────┬─────┘                            │
     │             │                                  │
     │     counter >= threshold                       │
     │             │                                  │
     │             ▼                                  │
     │        ┌──────────┐                            │
     └────────│  STABLE  │────────────────────────────┘
              │ (LOW)    │       current == 1, counter >= threshold
              └──────────┘       → Release-Edge (nicht verwendet)
                    │
                    │ current == 0
                    │
                    └─────────────────► Press-Edge erkannt!
```

### Zusammenfassung

> **Merkregel:** „Debounce = Zeitfilter. Threshold = Debounce-Zeit / Scan-Intervall. Nur stabile Zustände zählen."

---

## 8) One-Hot-Encoding – Die Invariante

### Problemstellung

Bei einem Selection Panel soll immer genau eine Auswahl aktiv sein (oder keine). Wie stellst du sicher, dass niemals zwei LEDs gleichzeitig leuchten – auch bei Bugs oder Race Conditions?

### Regel

One-Hot ist eine Invariante, die durch Konstruktion garantiert wird: Zuerst alle Bits löschen, dann genau eines setzen.

### Herleitung: One-Hot-Prinzip

| selectedId | Binär (2 Bytes, MSB-first) | Hex         |
|------------|----------------------------|-------------|
| 0          | `00000000 00000000`        | `0x00 0x00` |
| 1          | `10000000 00000000`        | `0x80 0x00` |
| 2          | `01000000 00000000`        | `0x40 0x00` |
| 8          | `00000001 00000000`        | `0x01 0x00` |
| 9          | `00000000 10000000`        | `0x00 0x80` |
| 10         | `00000000 01000000`        | `0x00 0x40` |

**Invariante:** Zu jedem Zeitpunkt gilt: `popcount(ledState) ≤ 1`

(popcount = Anzahl gesetzter Bits)

### Anwendung: Sichere Implementierung

```c
/**
 * Setzt LED-Zustand im One-Hot-Format.
 *
 * Garantiert: Entweder 0 oder genau 1 Bit gesetzt.
 *
 * @param selectedId  0 = alle aus, 1..LED_COUNT = diese LED an
 * @param ledState    Output-Array (wird überschrieben)
 * @param bytes       Größe des Arrays
 */
void setOneHot(uint8_t selectedId, uint8_t* ledState, size_t bytes) {
    // Schritt 1: ALLE Bits löschen (garantiert sauberen Zustand)
    memset(ledState, 0x00, bytes);

    // Schritt 2: Validierung der ID
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

**Alternativer Ansatz (unsicher):**

```c
// ✗ GEFÄHRLICH: Race Condition möglich
void setOneHot_UNSAFE(uint8_t oldId, uint8_t newId, uint8_t* ledState) {
    // Schritt 1: Altes Bit löschen
    clearBit(ledState, oldId);

    // Schritt 2: Neues Bit setzen
    setBit(ledState, newId);
}
```

**Problem:** Zwischen Schritt 1 und 2 könnte ein Interrupt die LED-Daten senden – der Empfänger sieht kurz 0 gesetzte Bits.

**Oder schlimmer:** Ein Bug setzt `oldId` falsch, und plötzlich hast du 2 Bits gesetzt.

**Mit memset + einem |= ist die Invariante garantiert:**

1. `memset` → 0 Bits gesetzt
2. `|=` → genau 1 Bit gesetzt
3. Kein Zustand mit 2+ Bits möglich

### Anwendung: Bit-Position-Berechnung (MSB-first)

```
selectedId = 9, LED_COUNT = 10, LED_BYTES = 2

byteIdx = (9 - 1) / 8 = 8 / 8 = 1  → Byte[1]
bitPos  = 7 - ((9 - 1) % 8)
        = 7 - (8 % 8)
        = 7 - 0
        = 7                        → Bit 7 (MSB)

ledState[1] |= (1u << 7) = 0b10000000 = 0x80

Ergebnis: Byte[0] = 0x00, Byte[1] = 0x80
```

### Zusammenfassung

> **Merkregel:** „One-Hot = Clear-then-Set. memset(0) garantiert die Invariante, |= setzt genau ein Bit."

---

## 9) SSOT – Single Source of Truth

### Problemstellung

Wenn Pin-Definitionen in drei Dateien stehen (main.cpp, driver.cpp, test.cpp) und du eine änderst, sind die anderen falsch. Das System verhält sich unerklärlich.

### Regel

Jede Information hat genau eine autoritative Quelle. Alle anderen Stellen referenzieren diese Quelle.

### Anwendung im Projekt

| Aspekt              | SSOT-Datei                         | Konsumenten             |
|---------------------|------------------------------------|-------------------------|
| Pin-Zuordnung       | `firmware/include/hw/board_pins.h` | hal/, drivers/          |
| Kanalanzahl, Masken | `firmware/include/hw/io_counts.h`  | logic/, drivers/, tests |
| SPI-Parameter       | `firmware/include/hw/spi_params.h` | hal/spi_bus.cpp         |

**Beispiel – io_counts.h:**

```c
// firmware/include/hw/io_counts.h
// SSOT für Kanalanzahl – bei Änderung nur hier editieren!

#define BTN_COUNT       10
#define LED_COUNT       10

// Abgeleitete Werte (automatisch korrekt bei Änderung von COUNT)
#define BTN_BYTES       ((BTN_COUNT + 7) / 8)   // = 2
#define LED_BYTES       ((LED_COUNT + 7) / 8)   // = 2

// Maske für gültige Bits im letzten Byte
// Bei 10 LEDs: 16 Bits verfügbar, 10 genutzt
// Byte[1]: Bits 7, 6 gültig → Maske = 0xC0
#define LED_LAST_MASK   0xC0
```

### Herleitung: Änderung von 10 auf 100 Kanäle

**Änderungen in io_counts.h:**

```c
#define BTN_COUNT       100
#define LED_COUNT       100

// BTN_BYTES = (100 + 7) / 8 = 13 (automatisch)
// LED_BYTES = (100 + 7) / 8 = 13 (automatisch)

// LED_LAST_MASK muss manuell berechnet werden:
// 100 % 8 = 4 → Bits 7, 6, 5, 4 gültig → Maske = 0xF0
#define LED_LAST_MASK   0xF0
```

**Keine weiteren Änderungen nötig in:**
- Treiber-Code (nutzt `BTN_BYTES`, `LED_BYTES`)
- Logik-Code (nutzt `BTN_COUNT`, `LED_COUNT`)
- Test-Code (inkludiert io_counts.h)

### Zusammenfassung

> **Merkregel:** „Eine Zahl, eine Stelle. Änderst du SSOT, änderst du überall."

---

## 10) Test-Strategie – Vertrauen durch Verifikation

### Problemstellung

Embedded-Bugs sind schwer zu debuggen. Du siehst nicht, was auf der Leitung passiert. Ohne Oszilloskop oder Logic Analyzer ist Fehlersuche Raterei.

### Regel

Zweistufige Test-Strategie: Logik isoliert testen (Unit-Tests), dann Hardware verifizieren (E2E-Tests).

### Anwendung: Unit-Tests (Logik isoliert)

**Befehl:**

```bash
pio test -d firmware -e seeed_xiao_esp32s3_test -v
```

**Was wird getestet:**
- Debounce-Algorithmus (filtert Prellen korrekt?)
- Selection-Logik (One-Hot-Invariante garantiert?)
- Bit-Mapping (ID → Byte/Bit korrekt?)

**Beispiel-Test:**

```c
void test_debounce_filters_noise() {
    DebounceState state = {.stable = 1, .counter = 0};

    // Simuliere Prellen: 1 → 0 → 1 → 0 → 0 → 0
    TEST_ASSERT_FALSE(debounce_update(&state, 0, 3));  // counter = 1
    TEST_ASSERT_FALSE(debounce_update(&state, 1, 3));  // counter = 0 (reset)
    TEST_ASSERT_FALSE(debounce_update(&state, 0, 3));  // counter = 1
    TEST_ASSERT_FALSE(debounce_update(&state, 0, 3));  // counter = 2
    TEST_ASSERT_TRUE(debounce_update(&state, 0, 3));   // counter = 3 → Edge!

    TEST_ASSERT_EQUAL(0, state.stable);  // Neuer stabiler Zustand
}

void test_oneHot_sets_single_bit() {
    uint8_t ledState[2] = {0xFF, 0xFF};  // Absichtlich "verschmutzt"

    setOneHot(1, ledState, 2);
    TEST_ASSERT_EQUAL_HEX8(0x80, ledState[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, ledState[1]);

    setOneHot(10, ledState, 2);
    TEST_ASSERT_EQUAL_HEX8(0x00, ledState[0]);
    TEST_ASSERT_EQUAL_HEX8(0x40, ledState[1]);

    setOneHot(0, ledState, 2);  // Alle aus
    TEST_ASSERT_EQUAL_HEX8(0x00, ledState[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, ledState[1]);
}
```

**Vorteil:** Läuft in Sekunden, keine Hardware nötig. Schnelles Feedback bei Refactoring.

### Anwendung: E2E-Tests (Hardware verifiziert)

**Befehl:**

```bash
pio test -d firmware -e seeed_xiao_esp32s3_e2e -f test_e2e_hw -v
```

**Was wird getestet:**
- SPI-Timing (Daten kommen korrekt an?)
- Pin-Zuordnung (Button 1 → Bit 7 von Byte 0?)
- LED-Mapping (selectedId=5 → LED 5 leuchtet?)

**Typischer Ablauf (interaktiv):**

| Schritt | Aktion                | Erwartung                  |
|---------|-----------------------|----------------------------|
| 1       | Nichts drücken (Idle) | RAW = 0xFF (alle HIGH)     |
| 2       | Button 1 drücken      | RAW Bit 7 = 0              |
| 3       | Button 1 loslassen    | RAW = 0xFF                 |
| 4       | Button 10 drücken     | RAW Byte 1, Bit 6 = 0      |
| 5       | Walking-One LEDs      | Visuell prüfen: 1, 2, 3... |

### Herleitung: Wann welcher Test?

| Änderung           | Unit-Test | E2E-Test | Begründung                    |
|--------------------|:---------:|:--------:|-------------------------------|
| Debounce-Threshold |     ✓     |    –     | Reine Logik                   |
| Selection-Policy   |     ✓     |    –     | Reine Logik                   |
| Bit-Mapping-Formel |     ✓     |    ✓     | Logik + Hardware-Verifikation |
| SPI-Frequenz       |     –     |    ✓     | Nur mit Hardware testbar      |
| Pin-Änderung       |     –     |    ✓     | Physische Verdrahtung         |
| Neuer Message-Type |     ✓     |    ✓     | Logik + Integration           |

