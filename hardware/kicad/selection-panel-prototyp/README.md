# Selection Panel Prototyp – KiCad 8 Projekt

## Projektinhalt

```
selection-panel-prototyp/
├── selection-panel-prototyp.kicad_pro   ← Projekt öffnen
├── selection-panel-prototyp.kicad_sch   ← Schaltplan (leer, zum Befüllen)
├── selection-panel-prototyp.kicad_pcb   ← PCB (leer, Netze vordefiniert)
└── selection-panel-prototyp.net         ← Original-Netzliste (Referenz)
```

## Workflow in KiCad 8

### 1. Projekt öffnen

```
Datei → Projekt öffnen → selection-panel-prototyp.kicad_pro
```

### 2. Symbole hinzufügen

KiCad 8 unterstützt keinen Netzlisten-Import mehr. Die Bauteile müssen manuell platziert werden:

1. **Schematic Editor** öffnen (Doppelklick auf `.kicad_sch`)
2. `A` drücken → Symbol suchen und platzieren

| Bauteil | Suchbegriff | Bibliothek |
|---------|-------------|------------|
| 74HC595 | `74HC595` | 74xx |
| CD4021 | `4021` | 4xxx |
| LED | `LED` | Device |
| Widerstand | `R` | Device |
| Kondensator | `C` | Device |
| Taster | `SW_Push` | Switch |
| ESP32 | `Conn_01x14` | Connector (als Platzhalter) |

### 3. Verbindungen zeichnen

Drähte ziehen mit `W`, oder Labels nutzen (`L`):

| Label | Von | Nach |
|-------|-----|------|
| LED_DATA | U1 Pin 1 (D0) | U2 Pin 14 (SER) |
| LED_CLOCK | U1 Pin 2 (D1) | U2/U3 Pin 11 (SRCLK) |
| LED_LATCH | U1 Pin 3 (D2) | U2/U3 Pin 12 (RCLK) |
| LED_CASCADE | U2 Pin 9 (QH') | U3 Pin 14 (SER) |
| BTN_DATA | U4 Pin 3 (Q8) | U1 Pin 4 (D3) |
| BTN_CLOCK | U1 Pin 5 (D4) | U4/U5 Pin 10 (CLK) |
| BTN_LOAD | U1 Pin 6 (D5) | U4/U5 Pin 9 (P/S) |
| BTN_CASCADE | U5 Pin 3 (Q8) | U4 Pin 11 (SER) |

### 3. Annotation prüfen

```
Werkzeuge → Schaltplansymbole annotieren → Annotate
```

### 4. ERC ausführen

```
Prüfen → Electrical Rules Checker → Run ERC
```

### 5. PCB aktualisieren

```
Werkzeuge → Update PCB from Schematic (F8)
```

Die Footprints erscheinen gestapelt. Dann:

1. Bauteile platzieren (M = verschieben, R = rotieren)
2. Leiterbahnen routen mit X
3. GND-Fläche hinzufügen

## Kritische Verbindungen

Diese Pins müssen fest verdrahtet werden:

| IC | Pin | Ziel | Begründung |
|----|-----|------|------------|
| U2, U3 | 10 (SRCLR) | VCC | Reset deaktiviert |
| U2, U3 | 13 (OE) | GND | Ausgänge aktiv |
| U5 | 11 (SER) | VCC | CMOS darf nicht floaten |

## Bauteilliste

| Ref | Wert | Footprint |
|-----|------|-----------|
| U1 | ESP32-S3-XIAO | Seeed:XIAO-ESP32S3 |
| U2, U3 | 74HC595 | Package_DIP:DIP-16_W7.62mm |
| U4, U5 | CD4021BE | Package_DIP:DIP-16_W7.62mm |
| D1–D10 | LED | LED_THT:LED_D5.0mm |
| R1–R10 | 330Ω | Resistor_THT:R_Axial_DIN0207 |
| R11–R20 | 10kΩ | Resistor_THT:R_Axial_DIN0207 |
| C1–C4 | 100nF | Capacitor_THT:C_Disc_D5.0mm |
| SW1–SW10 | Taster | Button_Switch_THT:SW_PUSH_6mm |

---

*KiCad 8.0 | Dezember 2025*
