# Glossar: Embedded-Entwicklung

## A

### ACK (Acknowledge)

**Definition:** Bestätigung einer empfangenen Nachricht im Request-Response-Protokoll.

**Projektbezug:** Die Bridge sendet ACK nach erfolgreichem Empfang eines Kommandos. Das ACK-Paket enthält die Sequenznummer des bestätigten Pakets.

**Protokoll-Ablauf:**

```
Sender                          Empfänger
   │                                │
   │── Paket (seq=42, ACK_REQ) ────►│
   │                                │ (verarbeitet)
   │◄────── ACK (seq=42) ───────────│
   │                                │
```

**Verwandte Begriffe:** → NACK, → Retry, → Timeout

---

### Active-High

**Definition:** Signal-Konvention, bei der der aktive Zustand dem HIGH-Pegel (VCC) entspricht.

| Zustand | Pegel | Spannung (3,3 V Logik) |
|---------|-------|------------------------|
| Aktiv   | HIGH  | 2,0 V – 3,3 V          |
| Inaktiv | LOW   | 0 V – 0,8 V            |

**Projektbezug:** LEDs am 74HC595 sind Active-High – ein gesetztes Bit (1) schaltet die LED ein.

**Code-Muster:**

```c
// LED einschalten (Active-High)
ledState |= (1u << bitPos);   // Bit = 1 → LED an

// LED ausschalten
ledState &= ~(1u << bitPos);  // Bit = 0 → LED aus
```

**Verwandte Begriffe:** → Active-Low, → Pull-up

---

### Active-Low

**Definition:** Signal-Konvention, bei der der aktive Zustand dem LOW-Pegel (GND) entspricht.

| Zustand | Pegel | Spannung (3,3 V Logik) |
|---------|-------|------------------------|
| Aktiv   | LOW   | 0 V – 0,8 V            |
| Inaktiv | HIGH  | 2,0 V – 3,3 V          |

**Projektbezug:** CD4021-Buttons sind Active-Low – ein gedrückter Taster zieht den Eingang auf GND (0).

**Code-Muster:**

```c
// Button-Zustand prüfen (Active-Low)
bool isPressed = (rawByte & mask) == 0;  // Bit = 0 → gedrückt
```

**Warum Active-Low für Taster?**
- Pull-up-Widerstand definiert Ruhezustand (HIGH)
- Taster schaltet nur nach GND – einfacher und robuster
- Historisch: TTL-Logik hatte stärkere LOW-Treiber

**Verwandte Begriffe:** → Active-High, → Pull-up, → CD4021

---

## B

### Baudrate

**Definition:** Übertragungsgeschwindigkeit einer seriellen Verbindung in Symbolen pro Sekunde. Bei binärer Übertragung: 1 Baud ≙ 1 Bit/s.

**Typische Werte:**

| Baudrate | Zeichen/s | Anwendung       |
|----------|-----------|-----------------|
| 9600     | 960       | Legacy, Debug   |
| 115200   | 11520     | Standard Serial |
| 921600   | 92160     | High-Speed      |

**Projektbezug:** USB-CDC mit nominell 921600 Baud. Bei USB-CDC wird die Baudrate vom USB-Stack ignoriert – die tatsächliche Geschwindigkeit hängt vom USB-Durchsatz ab.

**Berechnung Übertragungszeit:**

$$t_{\text{Byte}} = \frac{10 \text{ Bits}}{\text{Baudrate}} = \frac{10}{115200} \approx 87\,\mu\text{s}$$

(10 Bits = 1 Start + 8 Daten + 1 Stop)

**Verwandte Begriffe:** → USB-CDC, → Frame

---

### Bit-Mapping

**Definition:** Zuordnung von logischen Identifikatoren (IDs) zu physischen Bit-Positionen in einem Byte-Array.

**Projektbezug:** MSB-first für Buttons und LEDs.

**Mapping-Tabelle (10 Kanäle):**

| ID | Byte-Index | Bit-Position | Maske |
|----|------------|--------------|-------|
| 1  | 0          | 7 (MSB)      | 0x80  |
| 2  | 0          | 6            | 0x40  |
| 8  | 0          | 0 (LSB)      | 0x01  |
| 9  | 1          | 7 (MSB)      | 0x80  |
| 10 | 1          | 6            | 0x40  |

**Berechnungsformel:**

```c
uint8_t byteIdx = (id - 1) / 8;
uint8_t bitPos  = 7 - ((id - 1) % 8);  // MSB-first
uint8_t mask    = 1u << bitPos;
```

**Verwandte Begriffe:** → LSB/MSB, → Maske

---

## C

### CD4021 (CD4021B)

**Definition:** 8-Bit CMOS Parallel-In/Serial-Out (PISO) Schieberegister von Texas Instruments.

**Funktion:** Wandelt 8 parallele Eingänge in serielle Daten für SPI-Übertragung.

**Pinbelegung (relevante Pins):**

| Pin | Name  | Funktion                       |
|-----|-------|--------------------------------|
| 3   | P/S   | Parallel/Serial Control (Load) |
| 10  | CLK   | Clock (Shift)                  |
| 3   | Q7    | Serial Data Out                |
| 1-8 | D0-D7 | Parallel Data Inputs           |

**Timing-Ablauf:**

```
P/S  ────┐    ┌─────────────────────────────
         └────┘  (kurzer HIGH-Puls zum Laden)
              │
CLK  ─────────┴──┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐
                 └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘
                   D7  D6  D5  D4  D3  D2  D1  D0
```

**Projektbezug:** Liest Button-Zustände (Active-Low). SPI Mode 1, 500 kHz.

**⚠ Häufiger Fehler:** Falscher SPI-Mode führt zu um ein Bit verschobenen Daten.

**Verwandte Begriffe:** → HC595, → SPI, → Active-Low

---

### COBS (Consistent Overhead Byte Stuffing)

**Definition:** Framing-Verfahren, das garantiert, dass ein bestimmtes Byte (0x00) nicht in den Nutzdaten vorkommt. Ermöglicht eindeutige Frame-Grenzen.

**Prinzip:**

```
Original:     [0x11] [0x00] [0x22] [0x00] [0x33]
                      ↓            ↓
COBS-kodiert: [0x02] [0x11] [0x02] [0x22] [0x02] [0x33]
               └─ Zeiger auf nächste "Null" oder Ende
```

**Overhead:** Maximal 1 Byte pro 254 Originalbytes.

**Projektbezug:** Serial-Protokoll zwischen ESP32 und Pi verwendet COBS mit 0x00 als Delimiter.

**Wire-Frame:**

```
┌────────────────────────────────┬──────┐
│  COBS-kodierte Daten           │ 0x00 │
│  (keine 0x00 enthalten)        │ Delim│
└────────────────────────────────┴──────┘
```

**Verwandte Begriffe:** → Frame, → CRC, → Wire Frame

---

### CRC (Cyclic Redundancy Check)

**Definition:** Prüfsummenverfahren zur Erkennung von Übertragungsfehlern basierend auf Polynomdivision.

**Projektbezug:** CRC-16/CCITT-FALSE

| Parameter | Wert   |
|-----------|--------|
| Polynom   | 0x1021 |
| Init      | 0xFFFF |
| XorOut    | 0x0000 |
| RefIn/Out | false  |

**Fehlererkennungsrate:**

| Fehlertyp             | Erkennungsrate |
|-----------------------|----------------|
| Alle Einzelbitfehler  | 100%           |
| Alle Doppelbitfehler  | 100%           |
| Burst-Fehler ≤ 16 Bit | 100%           |
| Zufällige Fehler      | 99,998%        |

**Verwandte Begriffe:** → Frame, → COBS

---

## D

### Debounce (Entprellung)

**Definition:** Filterung der mechanischen Prellvorgänge eines Tasters durch zeitliche Verzögerung.

**Problem – Prellen:**

```
Ideal:   ────┐          ┌────
             └──────────┘

Real:    ────┐ ┌┐ ┌┐    ┌┐┌──
             └─┘└─┘└────┘└┘
              ← Prellen →
```

**Algorithmus:** Ein Zustandswechsel wird erst akzeptiert, wenn er für N aufeinanderfolgende Samples stabil ist.

**Projektbezug:** 50 ms Debounce-Zeit bei 10 ms Scan-Intervall → Threshold = 5.

**Formel:**

$$\text{Threshold} = \frac{t_{\text{Debounce}}}{t_{\text{Scan}}} = \frac{50\,\text{ms}}{10\,\text{ms}} = 5$$

**Verwandte Begriffe:** → Edge, → Scan-Zyklus, → Guard

---

### Datenfluss

**Definition:** Die gerichtete Sequenz von Verarbeitungsschritten, durch die Daten fließen.

**Projektbezug – IO-Pipeline:**

```
CD4021::read()  →  Debounce  →  Selection  →  setOneHot()  →  HC595::write()
     │                │             │              │               │
  rawButtons     pressEdges    selectedId      ledState[]      Hardware
```

**Regel:** Daten fließen immer in eine Richtung. Keine Rückwärts-Abhängigkeiten innerhalb eines Zyklus.

**Verwandte Begriffe:** → Pipeline, → Scan-Zyklus

---

## E

### E2E-Test (End-to-End-Test)

**Definition:** Test, der das gesamte System von Eingang bis Ausgang mit echter Hardware prüft.

**Projektbezug:**

```bash
pio test -d firmware -e seeed_xiao_esp32s3_e2e -f test_e2e_hw -v
```

**Prüfpunkte:**

| Test                  | Erwartung                  |
|-----------------------|----------------------------|
| Idle (nichts drücken) | RAW = 0xFF                 |
| Button 1 drücken      | RAW Bit 7 = 0              |
| Walking-One LEDs      | LEDs leuchten nacheinander |

**Verwandte Begriffe:** → Unit-Test, → HAL

---

### Edge (Flanke)

**Definition:** Der Übergang eines Signals von einem Zustand zum anderen.

| Typ          | Übergang   | Verwendung                         |
|--------------|------------|------------------------------------|
| Rising Edge  | 0 → 1      | Clock-Sampling (Mode 0)            |
| Falling Edge | 1 → 0      | Clock-Sampling (Mode 1)            |
| Press-Edge   | HIGH → LOW | Button wurde gedrückt (Active-Low) |
| Release-Edge | LOW → HIGH | Button wurde losgelassen           |

**Projektbezug:** Die Selection-Logik reagiert auf Press-Edges, nicht auf Pegel.

**Warum Edge statt Pegel?**

```
Pegel-basiert:  Button gehalten → 100 Events/s bei 10 ms Scan
Edge-basiert:   Button gehalten → 1 Event (Press-Edge)
```

**Verwandte Begriffe:** → Debounce, → Polling

---

## F

### Frame

**Definition:** Ein abgegrenztes Datenpaket mit definiertem Anfang, Ende und Struktur.

**Projektbezug – Paket-Struktur:**

```
┌──────────────────────────────────────────────────────┐
│ Header (10 B) │ Payload (0-256 B) │ CRC-16 (2 B)    │
└──────────────────────────────────────────────────────┘
```

**Header-Felder:**

| Offset | Feld    | Größe | Beschreibung          |
|--------|---------|-------|-----------------------|
| 0-1    | Magic   | 2     | "SP" (0x53 0x50)      |
| 2-3    | Version | 2     | Major.Minor           |
| 4      | Type    | 1     | Message-Typ           |
| 5      | Flags   | 1     | ACK_REQ, IS_ACK, etc. |
| 6-7    | Seq     | 2     | Sequenznummer         |
| 8-9    | Len     | 2     | Payload-Länge         |

**Verwandte Begriffe:** → COBS, → CRC, → Wire Frame

---

### FreeRTOS

**Definition:** Real-Time Operating System für Mikrocontroller mit präemptivem Multitasking.

**Kernkonzepte:**

| Konzept   | Beschreibung                                     |
|-----------|--------------------------------------------------|
| Task      | Unabhängige Ausführungseinheit mit eigenem Stack |
| Queue     | Thread-sichere Nachrichtenwarteschlange          |
| Semaphore | Synchronisationsprimitiv                         |
| Mutex     | Gegenseitiger Ausschluss für Ressourcen          |

**Projektbezug:** Optional für Task-basierte Architektur (`APP_USE_FREERTOS=1`).

**Timing-Funktion:**

```c
// Präzises periodisches Timing
void ioTask(void* param) {
    TickType_t lastWake = xTaskGetTickCount();
    while (true) {
        // IO-Verarbeitung...
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(10));
    }
}
```

**Verwandte Begriffe:** → Task, → RTOS, → ISR

---

## G

### GPIO (General Purpose Input/Output)

**Definition:** Universell konfigurierbarer Mikrocontroller-Pin für digitale Ein- oder Ausgabe.

**Konfigurationsmodi:**

| Modus           | Beschreibung                          |
|-----------------|---------------------------------------|
| Input           | Liest externen Pegel                  |
| Input + Pull-up | Mit internem Pull-up-Widerstand       |
| Output          | Treibt Pegel aktiv                    |
| Open-Drain      | Kann nur LOW treiben, HIGH = floating |

**Projektbezug:** SPI-Pins, PS/LOAD, LATCH sind als GPIO konfiguriert.

**Verwandte Begriffe:** → Pull-up, → HAL

---

### Guard (Press-Guard)

**Definition:** Zeitfenster, das nach einem Event weitere Aktionen blockiert, um versehentliche Mehrfachauslösung zu verhindern.

**Projektbezug:** Press-Guard verhindert Doppelklicks innerhalb von z. B. 200 ms.

**Ablauf:**

```
Button gedrückt → Event ausgelöst → Guard aktiv (200 ms)
                                         │
Button erneut gedrückt ──────────────────►│ ignoriert
                                         │
                                    Guard endet
                                         │
Button erneut gedrückt ──────────────────►│ Event ausgelöst
```

**Verwandte Begriffe:** → Debounce, → Edge

---

## H

### HAL (Hardware Abstraction Layer)

**Definition:** Software-Schicht, die hardware-spezifische Details von der Anwendungslogik trennt.

**Projektbezug:** `firmware/src/hal/` enthält SPI-Zugriff.

**Schichtenmodell:**

```
┌─────────────────────────────┐
│  Anwendungslogik            │  ← Hardware-unabhängig
├─────────────────────────────┤
│  Treiber (CD4021, HC595)    │  ← IC-spezifisch
├─────────────────────────────┤
│  HAL (SPI, GPIO)            │  ← Plattform-spezifisch
├─────────────────────────────┤
│  Register / Hardware        │
└─────────────────────────────┘
```

**Vorteil:** Logik kann auf Host-PC getestet werden, ohne echte Hardware.

**Verwandte Begriffe:** → GPIO, → SPI

---

### HC595 (74HC595)

**Definition:** 8-Bit CMOS Serial-In/Parallel-Out (SIPO) Schieberegister mit Latch.

**Funktion:** Wandelt serielle SPI-Daten in 8 parallele Ausgänge.

**Pinbelegung (relevante Pins):**

| Pin     | Name  | Funktion                   |
|---------|-------|----------------------------|
| 14      | SER   | Serial Data In             |
| 11      | SRCLK | Shift Register Clock       |
| 12      | RCLK  | Register Clock (Latch)     |
| 13      | OE    | Output Enable (Active-Low) |
| 15, 1-7 | QA-QH | Parallel Outputs           |

**Timing-Ablauf:**

```
SER   ──D7──D6──D5──D4──D3──D2──D1──D0────────
           │    │    │    │    │    │    │    │
SRCLK ─────┴────┴────┴────┴────┴────┴────┴────┴─
                                              │
RCLK  ────────────────────────────────────────┴──┐ ┌──
                                                 └─┘
                                              Latch-Puls
```

**Projektbezug:** Steuert LEDs (Active-High). SPI Mode 0, 1 MHz.

**⚠ Häufiger Fehler:** Latch-Puls vergessen → LEDs zeigen alten Zustand.

**Verwandte Begriffe:** → CD4021, → Latch, → SPI

---

## I

### ISR (Interrupt Service Routine)

**Definition:** Funktion, die bei Auftreten eines Hardware-Interrupts ausgeführt wird.

**Regeln für ISRs:**

| ✓ Erlaubt                    | ✗ Verboten              |
|------------------------------|-------------------------|
| Flag setzen                  | printf/Serial.print     |
| Queue senden (FromISR)       | malloc/free             |
| Kurze Berechnungen           | Lange Schleifen         |
| Volatile-Variablen schreiben | Blockierende Funktionen |

**Code-Muster:**

```c
volatile bool buttonPressed = false;

void IRAM_ATTR buttonISR() {
    buttonPressed = true;  // Nur Flag setzen
}

void loop() {
    if (buttonPressed) {
        buttonPressed = false;
        handleButtonPress();  // Verarbeitung im Hauptkontext
    }
}
```

**Verwandte Begriffe:** → Volatile, → FreeRTOS

---

## L

### Latch

**Definition:** Speicherelement, das Daten bei einem Trigger-Signal übernimmt und stabil hält.

**Projektbezug:** Der 74HC595 hat einen internen Latch zwischen Schieberegister und Ausgängen.

**Warum Latch?**

```
Ohne Latch:  Ausgänge ändern sich während des Schiebens → Flackern
Mit Latch:   Ausgänge bleiben stabil bis zum Latch-Puls → Sauberer Wechsel
```

**Verwandte Begriffe:** → HC595, → SPI

---

### LSB / MSB

**Definition:**

| Begriff | Bedeutung             | Position       |
|---------|-----------------------|----------------|
| LSB     | Least Significant Bit | Bit 0 (rechts) |
| MSB     | Most Significant Bit  | Bit 7 (links)  |

**Visualisierung:**

```
Byte: 0b10110010
         │     │
        MSB   LSB
       Bit 7  Bit 0
      Wert:128 Wert:1
```

**Projektbezug:** MSB-first für Buttons und LEDs. ID 1 → Bit 7, ID 8 → Bit 0.

**Verwandte Begriffe:** → Bit-Mapping, → Maske

---

## M

### Maske (Bitmask)

**Definition:** Bitmuster zum Selektieren, Setzen oder Löschen bestimmter Bits.

**Operationen:**

| Operation   | Code             | Wirkung                    |
|-------------|------------------|----------------------------|
| Bit lesen   | `value & mask`   | Isoliert maskierte Bits    |
| Bit setzen  | `value |= mask`  | Setzt maskierte Bits auf 1 |
| Bit löschen | `value &= ~mask` | Setzt maskierte Bits auf 0 |
| Bit togglen | `value ^= mask`  | Invertiert maskierte Bits  |

**Beispiel:**

```c
uint8_t value = 0b11001010;
uint8_t mask  = 0b00001111;

value & mask   // = 0b00001010 (untere 4 Bits)
value | mask   // = 0b11001111 (untere 4 Bits gesetzt)
value & ~mask  // = 0b11000000 (untere 4 Bits gelöscht)
```

**Verwandte Begriffe:** → Bit-Mapping, → LSB/MSB

---

### MISO / MOSI

**Definition:** SPI-Datenleitungen.

| Signal | Vollständig          | Richtung       | Funktion        |
|--------|----------------------|----------------|-----------------|
| MOSI   | Master Out, Slave In | Master → Slave | Daten zum Slave |
| MISO   | Master In, Slave Out | Slave → Master | Daten vom Slave |

**Projektbezug:**
- MOSI → 74HC595 (LED-Daten)
- MISO ← CD4021 (Button-Daten)

**Verwandte Begriffe:** → SPI, → HAL

---

## O

### OE (Output Enable)

**Definition:** Steuersignal, das die Ausgänge eines ICs aktiviert oder hochohmig schaltet.

**Projektbezug:** 74HC595 OE-Pin (meist Active-Low).

| OE-Pegel | Ausgänge              |
|----------|-----------------------|
| LOW      | Aktiv (treiben Pegel) |
| HIGH     | Hochohmig (Tri-State) |

**Anwendung:** OE dauerhaft auf GND für permanente Ausgabe, oder mit PWM für Dimming.

**Verwandte Begriffe:** → HC595, → Active-Low

---

### One-Hot

**Definition:** Codierung, bei der zu jedem Zeitpunkt höchstens ein Bit gesetzt ist.

**Gültige Zustände (2 Bytes):**

| selectedId | Binär               | Gültig?    |
|------------|---------------------|------------|
| 0          | `00000000 00000000` | ✓          |
| 1          | `10000000 00000000` | ✓          |
| 5          | `00001000 00000000` | ✓          |
| ???        | `10001000 00000000` | ✗ (2 Bits) |

**Projektbezug:** Immer nur eine LED aktiv – garantiert durch `setOneHot()`.

**Implementierung:**

```c
void setOneHot(uint8_t id, uint8_t* state, size_t bytes) {
    memset(state, 0x00, bytes);  // Alle löschen
    if (id >= 1 && id <= LED_COUNT) {
        // Genau ein Bit setzen
    }
}
```

**Verwandte Begriffe:** → Bit-Mapping, → Selection

---

## P

### Pipeline

**Definition:** Sequenz von Verarbeitungsstufen mit definierten Ein- und Ausgängen.

**Projektbezug:**

```
┌──────────────────────────────────────────────────────┐
│                    io_pipeline                       │
│  ┌────────┐   ┌────────┐   ┌────────┐   ┌────────┐  │
│  │  Read  │ → │Debounce│ → │  Guard │ → │ Select │  │
│  └────────┘   └────────┘   └────────┘   └────────┘  │
└──────────────────────────────────────────────────────┘
```

**Regel:** Jede Stufe hat genau eine Verantwortung. Keine Stufe überspringen.

**Verwandte Begriffe:** → Datenfluss, → Scan-Zyklus

---

### Polling

**Definition:** Regelmäßiges Abfragen eines Zustands in einer Schleife.

**Projektbezug:** 10 ms Scan-Intervall für Button-Abfrage.

**Polling vs. Interrupt:**

| Aspekt        | Polling          | Interrupt          |
|---------------|------------------|--------------------|
| Latenz        | ≤ Scan-Intervall | Sofort             |
| CPU-Last      | Konstant         | Nur bei Event      |
| Komplexität   | Einfach          | Höher (ISR-Regeln) |
| Determinismus | Vorhersagbar     | Variable Latenz    |

**Verwandte Begriffe:** → Scan-Zyklus, → ISR

---

### Pull-up / Pull-down

**Definition:** Widerstand, der einen offenen Pin auf definierten Pegel zieht.

| Typ       | Pegel bei offenem Pin | Typischer Wert |
|-----------|-----------------------|----------------|
| Pull-up   | HIGH (VCC)            | 10 kΩ          |
| Pull-down | LOW (GND)             | 10 kΩ          |

**Schaltbild (Pull-up):**

```
VCC (3,3 V)
    │
   ┌┴┐
   │ │ 10 kΩ
   └┬┘
    ├───► GPIO-Pin (HIGH wenn offen)
    │
   ╱  Taster
    │
   GND
```

**Projektbezug:** CD4021-Inputs haben externe Pull-ups für Active-Low-Logik.

**Verwandte Begriffe:** → Active-Low, → GPIO

---

## R

### RTOS (Real-Time Operating System)

**Definition:** Betriebssystem mit garantierten Reaktionszeiten für zeitkritische Anwendungen.

**Eigenschaften:**

| Merkmal         | Beschreibung                          |
|-----------------|---------------------------------------|
| Präemption      | Höher priorisierte Tasks unterbrechen |
| Determinismus   | Vorhersagbare Worst-Case-Latenz       |
| Task-Scheduling | Prioritätsbasiert                     |
| IPC             | Queues, Semaphoren, Mutexe            |

**Projektbezug:** FreeRTOS ist optional (`APP_USE_FREERTOS=1`).

**Verwandte Begriffe:** → FreeRTOS, → Task

---

## S

### Scan-Zyklus

**Definition:** Ein vollständiger Durchlauf: Eingänge lesen → Verarbeiten → Ausgänge schreiben.

**Projektbezug:**

```
┌─────────────────────────────────────────────────────┐
│  10 ms Scan-Intervall                               │
│                                                     │
│  CD4021 read ─┬─ Debounce ─┬─ Selection ─┬─ HC595  │
│     20 µs     │    5 µs    │    5 µs     │  20 µs  │
│               │            │             │         │
│  ◄────────────── ~50 µs ──────────────────►        │
│               Reserve: ~9950 µs (99,5%)             │
└─────────────────────────────────────────────────────┘
```

**Verwandte Begriffe:** → Pipeline, → Polling, → Timing

---

### SPI (Serial Peripheral Interface)

**Definition:** Synchroner Vollduplex-Bus mit 4 Signalen (SCK, MOSI, MISO, CS).

**Modi:**

| Mode | CPOL | CPHA | Clock Idle | Sample bei   |
|:----:|:----:|:----:|------------|--------------|
|  0   |  0   |  0   | LOW        | Rising Edge  |
|  1   |  0   |  1   | LOW        | Falling Edge |
|  2   |  1   |  0   | HIGH       | Falling Edge |
|  3   |  1   |  1   | HIGH       | Rising Edge  |

**Projektbezug:**

| IC     | Mode | Frequenz |
|--------|:----:|----------|
| CD4021 |  1   | 500 kHz  |
| HC595  |  0   | 1 MHz    |

**Verwandte Begriffe:** → MISO/MOSI, → HAL

---

### SSOT (Single Source of Truth)

**Definition:** Prinzip, dass jede Information an genau einer Stelle definiert ist.

**Projektbezug:**

| Information   | SSOT-Datei                     |
|---------------|--------------------------------|
| Pin-Zuordnung | `include/hw/board_pins.h`      |
| Kanalanzahl   | `include/hw/io_counts.h`       |
| SPI-Parameter | `include/hw/spi_params.h`      |
| Protokoll     | `apps/bridge/docs/PROTOCOL.md` |

**Regel:** Änderung = nur eine Datei editieren.

**Verwandte Begriffe:** → HAL

---

## T

### Task

**Definition:** Unabhängige Ausführungseinheit in einem RTOS mit eigenem Stack und Priorität.

**FreeRTOS-API:**

```c
// Task erstellen
xTaskCreate(ioTask, "IO", 4096, NULL, 5, NULL);

// Task-Funktion
void ioTask(void* param) {
    while (true) {
        // Arbeit...
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

**Projektbezug:** `io_task` für IO-Verarbeitung.

**Verwandte Begriffe:** → FreeRTOS, → RTOS

---

### Timing

**Definition:** Zeitliche Abfolge und Dauer von Signalen und Operationen.

**Kritische Timing-Aspekte:**

| Aspekt         | Zu schnell              | Zu langsam           |
|----------------|-------------------------|----------------------|
| SPI-Clock      | Setup/Hold verletzt     | Durchsatz sinkt      |
| Scan-Intervall | CPU-Überlastung         | Träge Reaktion       |
| Debounce       | Prellen nicht gefiltert | Verzögerte Erkennung |

**Projektbezug:** Timing-Budget einhalten (< 10 ms pro Scan).

**Verwandte Begriffe:** → Scan-Zyklus, → SPI

---

## U

### Unit-Test

**Definition:** Test einer einzelnen Komponente in Isolation, ohne Hardware-Abhängigkeiten.

**Projektbezug:**

```bash
pio test -d firmware -e seeed_xiao_esp32s3_test -v
```

**Getestete Komponenten:**
- Debounce-Algorithmus
- Selection-Logik
- Bit-Mapping
- One-Hot-Encoding

**Vorteil:** Läuft in Sekunden, reproduzierbar, automatisierbar.

**Verwandte Begriffe:** → E2E-Test

---

### USB-CDC (USB Communication Device Class)

**Definition:** USB-Geräteklasse für virtuelle serielle Schnittstellen.

**Eigenschaften:**

| Aspekt    | Wert                                |
|-----------|-------------------------------------|
| Baudrate  | Wird ignoriert (USB bestimmt Speed) |
| Pufferung | OS-abhängig                         |
| Latenz    | Variabel (USB-Polling)              |

**Projektbezug:** ESP32-S3 ↔ Raspberry Pi Verbindung.

**Verwandte Begriffe:** → Baudrate, → Frame

---

## V

### Volatile

**Definition:** C-Keyword, das dem Compiler signalisiert, dass eine Variable sich unerwartet ändern kann (durch ISR, Hardware, andere Threads).

**Verwendung:**

```c
volatile bool flag = false;

void ISR() {
    flag = true;  // Wird von ISR geschrieben
}

void main() {
    while (!flag) {
        // Ohne volatile könnte der Compiler
        // flag nur einmal lesen und cachen
    }
}
```

**⚠ Regel:** Variablen, die von ISRs oder Hardware geschrieben werden, müssen `volatile` sein.

**Verwandte Begriffe:** → ISR

---

## W

### Watchdog

**Definition:** Hardware-Timer, der bei Nicht-Rücksetzen einen System-Reset auslöst.

**Zweck:** Erkennung von Software-Hängern und automatische Wiederherstellung.

**Ablauf:**

```
System startet → Watchdog Timer startet (z.B. 5 s)
     │
     ├─► Normaler Betrieb: Watchdog regelmäßig zurücksetzen
     │                          │
     │                     Timer Reset → Kein Reset
     │
     └─► Software hängt: Kein Reset des Watchdogs
                              │
                         Timer läuft ab → System-Reset
```

**Verwandte Begriffe:** → RTOS, → ISR

---

### Wire Frame

**Definition:** Das tatsächliche Datenpaket auf dem physischen Medium (nach Encoding).

**Unterschied Raw vs. Wire:**

| Begriff    | Inhalt                            |
|------------|-----------------------------------|
| Raw Frame  | Header + Payload + CRC (vor COBS) |
| Wire Frame | COBS-kodiert + 0x00 Delimiter     |

**Größenbeispiel (SELECTION_STATE):**

| Stufe      | Größe     |
|------------|-----------|
| Payload    | 8 Bytes   |
| Raw Frame  | 20 Bytes  |
| Wire Frame | ~22 Bytes |

**Verwandte Begriffe:** → Frame, → COBS
