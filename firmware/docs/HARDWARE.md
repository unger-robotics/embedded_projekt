# Hardware-Dokumentation

Hardwarespezifikation fuer das Seeed Studio XIAO ESP32-S3 Board.

---

## Board-Uebersicht

![Seeed Studio XIAO ESP32-S3](Seeed%20Studio%20XIAO%20ESP32-S3.png)

### Spezifikationen

| Eigenschaft     | Wert                                     |
|-----------------|------------------------------------------|
| **MCU**         | ESP32-S3 (Xtensa LX7 Dual-Core, 240 MHz) |
| **Flash**       | 8 MB                                     |
| **PSRAM**       | 8 MB (optional)                          |
| **SRAM**        | 512 KB                                   |
| **WiFi**        | 802.11 b/g/n (2.4 GHz)                   |
| **Bluetooth**   | BLE 5.0                                  |
| **USB**         | USB-C (Native USB + UART)                |
| **Abmessungen** | 21 x 17.5 mm                             |

---

## Pinbelegung

### Front-Seite (Hauptpins)

| Pin | GPIO   | Funktion           | Projekt-Verwendung      |
|-----|--------|--------------------|-----------------------  |
| D0  | GPIO1  | ADC1_CH0, Touch    | LED_RCK (74HC595 Latch) |
| D1  | GPIO2  | ADC1_CH1, Touch    | BTN_PS (CD4021 P/S)     |
| D2  | GPIO3  | ADC1_CH2, Touch    | LED_OE (74HC595 OE)     |
| D3  | GPIO4  | ADC1_CH3, Touch    | (frei)                  |
| D4  | GPIO5  | ADC1_CH4, Touch    | (frei)                  |
| D5  | GPIO6  | ADC1_CH5, Touch    | (frei)                  |
| D6  | GPIO43 | TX (UART0)         | (reserviert)            |
| D7  | GPIO44 | RX (UART0)         | (reserviert)            |
| D8  | GPIO7  | SPI SCK            | SPI Clock (gemeinsam)   |
| D9  | GPIO8  | SPI MISO           | BTN_MISO (CD4021 Q8)    |
| D10 | GPIO9  | SPI MOSI           | LED_MOSI (74HC595 SER)  |
| 5V  | -      | 5V Eingang/Ausgang | -                       |
| GND | -      | Masse              | Gemeinsame Masse        |
| 3V3 | -      | 3.3V Ausgang       | VCC fuer Schieberegister|

### Rueck-Seite (Debug/Batterie)

| Pad  | Funktion | Beschreibung           |
|------|----------|------------------------|
| MTDO | GPIO40   | JTAG TDO               |
| MTDI | GPIO41   | JTAG TDI               |
| MTCK | GPIO39   | JTAG TCK               |
| MTMS | GPIO42   | JTAG TMS               |
| EN   | -        | Enable (Reset)         |
| D+   | -        | USB Data+              |
| D-   | -        | USB Data-              |
| BAT+ | -        | Batterie + (3.7V LiPo) |
| BAT- | -        | Batterie -             |

### Onboard-Komponenten

| Komponente       | GPIO   | Beschreibung            |
|------------------|--------|-------------------------|
| **User LED**     | GPIO21 | Gelbe LED (Active High) |
| **Charge LED**   | -      | Ladeanzeige (Orange)    |
| **Reset Button** | -      | Hardware-Reset          |
| **Boot Button**  | GPIO0  | Boot-Mode / User Button |

---

## Projekt-GPIO-Mapping

### Aktuelle Belegung (Selection Panel)

| Pin | Funktion     | Konstante      | Chip     | Beschreibung              |
|-----|--------------|----------------|----------|---------------------------|
| D0  | LED_RCK      | `PIN_LED_RCK`  | 74HC595  | Latch (RCLK)              |
| D1  | BTN_PS       | `PIN_BTN_PS`   | CD4021BE | Parallel/Shift            |
| D2  | LED_OE       | `PIN_LED_OE`   | 74HC595  | Output Enable (Active-Low)|
| D8  | SCK          | `PIN_SCK`      | Beide    | SPI Clock (gemeinsam)     |
| D9  | BTN_MISO     | `PIN_BTN_MISO` | CD4021BE | Serial Out (Q8)           |
| D10 | LED_MOSI     | `PIN_LED_MOSI` | 74HC595  | Serial In (SER)           |

Definiert in: `include/hw/board_pins.h`

### Schieberegister-Konfiguration

| Chip     | Anzahl | Funktion                | SPI-Frequenz | SPI-Mode |
|----------|--------|-------------------------|--------------|----------|
| CD4021BE | 2      | 16 Inputs (10 genutzt)  | 500 kHz      | MODE1    |
| 74HC595  | 2      | 16 Outputs (10 genutzt) | 1 MHz        | MODE0    |

Definiert in: `include/hw/spi_params.h`

### Reservierte GPIOs

| GPIO   | Grund         | Hinweis                        |
|--------|---------------|--------------------------------|
| GPIO0  | Boot Button   | Low beim Start = Download-Mode |
| GPIO43 | USB Serial TX | Native USB                     |
| GPIO44 | USB Serial RX | Native USB                     |
| GPIO19 | USB D-        | Nicht verwenden                |
| GPIO20 | USB D+        | Nicht verwenden                |

### Verfuegbare GPIOs fuer Erweiterung

| Pin | GPIO   | Empfohlene Verwendung   |
|-----|--------|-------------------------|
| D3  | GPIO4  | Digital I/O, Touch      |
| D4  | GPIO5  | Digital I/O, Touch      |
| D5  | GPIO6  | Digital I/O, Touch      |
| D6  | GPIO43 | (TX - reserviert)       |
| D7  | GPIO44 | (RX - reserviert)       |

---

## Schnittstellen

### USB (Native)

```
USB-C Connector
├── CDC Serial (921600 baud fuer Bridge-Protokoll)
├── Debug Monitor (115200 baud)
├── JTAG Debug (optional)
└── Mass Storage (optional)
```

**Hinweis:** USB-CDC ignoriert Baudrate, aber Bridge konfiguriert 921600. Debug-Monitor nutzt 115200.

### I2C (nicht verwendet)

| Signal | GPIO  | Pull-Up             |
|--------|-------|---------------------|
| SDA    | GPIO8 | Extern erforderlich |
| SCL    | GPIO7 | Extern erforderlich |

**Hinweis:** GPIO7/GPIO8 werden im Selection Panel fuer SPI verwendet.
I2C ist daher nicht gleichzeitig nutzbar.

### SPI (Selection Panel)

| Signal | Pin | GPIO  | Verwendung                    |
|--------|-----|-------|-------------------------------|
| SCK    | D8  | GPIO7 | Gemeinsamer Clock             |
| MISO   | D9  | GPIO8 | CD4021 Q8 (Button-Daten)      |
| MOSI   | D10 | GPIO9 | 74HC595 SER (LED-Daten)       |

Steuerleitungen (keine Hardware-CS):

| Signal   | Pin | GPIO  | Verwendung                    |
|----------|-----|-------|-------------------------------|
| BTN_PS   | D1  | GPIO2 | CD4021 Parallel/Shift         |
| LED_RCK  | D0  | GPIO1 | 74HC595 Latch                 |
| LED_OE   | D2  | GPIO3 | 74HC595 Output Enable         |

**Hinweis:** Beide Chips teilen SCK, aber verwenden separate Steuerleitungen.
Transfers erfolgen mit chip-spezifischen `spi_settings_t` (Frequenz/Mode).

---

## Stromversorgung

### Eingaenge

| Quelle | Spannung  | Max. Strom |
|--------|-----------|------------|
| USB-C  | 5V        | 500mA      |
| BAT+/- | 3.7V LiPo | -          |
| 5V Pin | 5V        | -          |

### Ausgaenge

| Pin | Spannung                 | Max. Strom         |
|-----|--------------------------|--------------------|
| 3V3 | 3.3V                     | 700mA              |
| 5V  | 5V (USB durchgeschleift) | begrenzt durch USB |

### Stromverbrauch (typisch)

| Modus             | Verbrauch   |
|-------------------|-------------|
| Active (WiFi)     | ~100-200 mA |
| Active (CPU only) | ~50 mA      |
| Light Sleep       | ~1 mA       |
| Deep Sleep        | ~14 µA      |

---

## Anschlussbild

```
                    USB-C
                   ┌─────┐
         ┌─────────┤     ├─────────┐
         │  [RST]  └─────┘  [BOOT] │
         │   ●                 ●   │
    D0 ──┤ ○ LED_RCK             ○ ├── 5V
    D1 ──┤ ○ BTN_PS              ○ ├── GND
    D2 ──┤ ○ LED_OE              ○ ├── 3V3
    D3 ──┤ ○ (frei)              ○ ├── D10 LED_MOSI
    D4 ──┤ ○ (frei)              ○ ├── D9  BTN_MISO
    D5 ──┤ ○ (frei)  [LED]       ○ ├── D8  SCK
    D6 ──┤ ○ (TX)    GPIO21      ○ ├── D7  (RX)
         └─────────────────────────┘

    ● = Button
    ○ = Pin
    [LED] = Onboard User LED (GPIO21)
```

### Verbindung zu Schieberegistern

```
XIAO ESP32-S3              CD4021BE (Buttons)       74HC595 (LEDs)
┌─────────────┐           ┌──────────────┐        ┌──────────────┐
│ D8 (SCK)   ─┼───────────┼─► CLK        │   ┌────┼─► SRCLK      │
│             │           │              │   │    │              │
│ D1 (BTN_PS)─┼───────────┼─► PS         │   │    │              │
│             │           │              │   │    │              │
│ D9 (MISO) ◄─┼───────────┼── Q8         │   │    │              │
│             │           └──────────────┘   │    │              │
│             │                              │    │              │
│ D8 (SCK)   ─┼──────────────────────────────┘    │              │
│             │                                   │              │
│ D10 (MOSI) ─┼───────────────────────────────────┼─► SER        │
│             │                                   │              │
│ D0 (RCK)   ─┼───────────────────────────────────┼─► RCLK       │
│             │                                   │              │
│ D2 (OE)    ─┼───────────────────────────────────┼─► OE         │
└─────────────┘                                   └──────────────┘
```

---

## Externe Hardware (Selection Panel)

### CD4021BE (Button-Input)

Parallel-In / Serial-Out Schieberegister fuer Button-Matrix.

| Pin | Funktion | Verbindung |
|-----|----------|------------|
| PS  | Parallel/Shift | D1 (HIGH=Load, LOW=Shift) |
| CLK | Clock | D8 (SPI SCK) |
| Q8  | Serial Out | D9 (MISO) |
| P1-P8 | Parallel Inputs | Buttons (Active-Low, Pull-Up) |
| VCC | Versorgung | 3.3V |
| GND | Masse | GND |

**Kaskadierung:** Q8 von Chip 1 → DS von Chip 2

### 74HC595 (LED-Output)

Serial-In / Parallel-Out Schieberegister fuer LED-Ansteuerung.

| Pin | Funktion | Verbindung |
|-----|----------|------------|
| SER | Serial In | D10 (MOSI) |
| SRCLK | Shift Clock | D8 (SPI SCK) |
| RCLK | Latch Clock | D0 |
| OE | Output Enable | D2 (Active-Low) |
| Q0-Q7 | Parallel Outputs | LEDs (Active-High) |
| VCC | Versorgung | 3.3V |
| GND | Masse | GND |

**Kaskadierung:** Q7' von Chip 1 → SER von Chip 2

### Beschaltung

| Komponente | Wert | Hinweis |
|------------|------|---------|
| Button Pull-Up | 10 kOhm | Extern oder intern |
| LED-Vorwiderstand | 220-470 Ohm | Je nach LED-Typ |
| Abblockkondensator | 100 nF | Je IC nahe VCC/GND |

---

## Hinweise

### Boot-Mode

| GPIO0 beim Start     | Modus         |
|----------------------|---------------|
| High (normal)        | Normal Boot   |
| Low (BOOT gedrueckt) | Download Mode |

### Antenna

- Onboard PCB-Antenne
- Externer U.FL Anschluss (XIAO ESP32-S3 Sense Variante)

### Waermemanagement

- Thermal Pad auf Rueckseite
- Bei hoher Last Kuehlung empfohlen

---

## Siehe auch

- `wiring.md` – Detaillierte Verdrahtungsanleitung
- `architecture.md` – Firmware-Schichten und Datenfluss
- `debug-playbook.md` – Hardware-Debugging
- `DEVELOPER.md` – Treiber-API (C API fuer cd4021/hc595)
