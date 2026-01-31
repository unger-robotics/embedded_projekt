# KiCad Anleitung für macOS

## Was ist eine Netzliste?

Eine Netzliste beschreibt die elektrischen Verbindungen zwischen Bauteilen – jedoch nicht deren visuelle Anordnung. Wenn wir uns die Datei `selection-panel-prototyp.net` ansehen, finden wir dort Bauteile (U1, R1, D1…), ihre Werte und alle elektrischen Netze. Was fehlt: Positionen, Leiterbahnen, Layout.

### Workflow

```
Netzliste (.net)
      │
      ▼
┌─────────────────┐
│ Eeschema        │ ← Schaltplan zeichnen (empfohlen)
└────────┬────────┘
         ▼
┌─────────────────┐
│ PCBnew          │ ← Bauteile platzieren, routen
└────────┬────────┘
         ▼
   Gerber-Dateien → Fertiger
```

Die Netzliste dient als Referenz. Für ein vollständiges Projekt empfehle ich, den Schaltplan in KiCad neu zu zeichnen – so bleibt alles wartbar.

---

## Installation

### Download und Setup

1. [kicad.org/download/macos](https://www.kicad.org/download/macos/) öffnen
2. KiCad 8.x herunterladen (ca. 2 GB)
3. `.dmg` öffnen, in Programme ziehen
4. Erster Start: Rechtsklick → Öffnen (Gatekeeper bestätigen)

**Alternative via Homebrew:**

```bash
brew install --cask kicad
```

### Bibliotheken konfigurieren

Beim ersten Start fragt KiCad nach Symbol- und Footprint-Bibliotheken. Die Standard-Einstellungen genügen für dieses Projekt – alle benötigten Bauteile (74HC595, Widerstände, LEDs) sind enthalten.

---

## Projekt anlegen

1. **Datei → Neues Projekt**
2. Namen eingeben: `selection-panel-prototyp`
3. Speichern

KiCad erstellt automatisch:

```
selection-panel-prototyp/
├── .kicad_pro   (Projektdatei)
├── .kicad_sch   (Schaltplan)
├── .kicad_pcb   (PCB-Layout)
└── gerber/      (für Fertigung)
```

Die Netzliste in denselben Ordner kopieren.

---

## Schaltplan zeichnen

### Tastenkürzel

| Taste | Aktion |
|-------|--------|
| A | Symbol hinzufügen |
| P | Power-Symbol (VCC, GND) |
| W | Draht |
| R | Rotieren |
| M | Verschieben |
| E | Eigenschaften |

### Symbole finden

| Bauteil | Suchbegriff | Bibliothek |
|---------|-------------|------------|
| 74HC595 | `74HC595` | 74xx |
| CD4021 | `4021` | 4xxx |
| LED | `LED` | Device |
| Widerstand | `R` | Device |
| Taster | `SW_Push` | Switch |

### ESP32-S3 XIAO

Das XIAO-Board ist nicht in den Standard-Bibliotheken. Zwei Optionen:

**Option A – Generisches Symbol:** `Conn_01x14` verwenden, als "ESP32-S3-XIAO" benennen, Pins beschriften (D0–D10, 3V3, GND, 5V).

**Option B – Custom Symbol:** Im Symbol-Editor neues Symbol mit den korrekten Pins erstellen.

### Schritt-für-Schritt

1. Power-Symbole platzieren (P drücken): VCC und GND
2. ICs setzen: U2, U3 (74HC595), U4, U5 (CD4021)
3. ESP32 platzieren
4. Verbindungen gemäß Netzliste zeichnen (W drücken)
5. LEDs mit Vorwiderständen: Qx → [330Ω] → [LED] → GND
6. Taster mit Pull-Ups: VCC → [10kΩ] → Px ← [SW] ← GND

### Annotation und ERC

Nach dem Zeichnen:

1. **Werkzeuge → Schaltplansymbole annotieren** – vergibt R1, R2, U1, U2…
2. **Prüfen → Electrical Rules Checker** – findet nicht verbundene Pins

---

## Netzliste importieren

Falls wir die Netzliste direkt nutzen möchten (ohne Schaltplan):

1. PCB-Editor öffnen
2. **Datei → Import → Netzliste**
3. `selection-panel-prototyp.net` auswählen

Die Bauteile erscheinen gestapelt. Verbindungen zeigt KiCad als Ratsnest (dünne Linien).

### Footprint-Zuordnung

Falls Footprints fehlen, über **Werkzeuge → Footprints zuordnen** nachbessern:

| Bauteil | Footprint |
|---------|-----------|
| 74HC595, CD4021 | Package_DIP:DIP-16_W7.62mm |
| LED 5mm | LED_THT:LED_D5.0mm |
| Widerstand | Resistor_THT:R_Axial_DIN0207 |
| Kondensator | Capacitor_THT:C_Disc_D5.0mm |
| Taster | Button_Switch_THT:SW_PUSH_6mm |

---

## PCB-Layout

### Setup

Unter **Datei → Platinen-Setup**:

- Lagenzahl: 2 (Top + Bottom)
- Leiterbahnbreite: 0.3 mm (Signal), 0.5 mm (Power)
- Abstand: 0.2 mm
- Via: 0.8 mm / 0.4 mm Bohrung

### Platinenumriss

Layer **Edge.Cuts** auswählen, Rechteck zeichnen. Für den Prototyp genügen 100 × 80 mm.

### Platzierung

```
┌─────────────────────────────────────────────────┐
│  [ESP32]                                        │
│                                                 │
│  [74HC595 #0]  [74HC595 #1]                    │
│                                                 │
│  [CD4021 #0]   [CD4021 #1]                     │
│                                                 │
│  ○ ○ ○ ○ ○ ○ ○ ○ ○ ○  ← LEDs                  │
│  □ □ □ □ □ □ □ □ □ □  ← Taster                │
└─────────────────────────────────────────────────┘
```

### Routing

| Taste | Aktion |
|-------|--------|
| X | Leiterbahn starten |
| V | Via einfügen |
| B | Kupferflächen füllen |

Strategie: VCC und GND mit 0.5 mm routen, Signale mit 0.3 mm. Am Ende GND-Fläche auf beiden Layern.

### Design Rules Check

**Prüfen → Design Rules Checker** ausführen. Typische Fehler:

- Unconnected items → fehlende Verbindung
- Clearance violation → Bahnen zu nah
- Track width → Bahn zu dünn

---

## Fertigung

### Gerber-Export

1. **Datei → Fertigungsdateien → Gerber**
2. Ausgabeverzeichnis: `gerber/`
3. Layer aktivieren: F.Cu, B.Cu, F.SilkS, B.SilkS, F.Mask, B.Mask, Edge.Cuts
4. Plot klicken

### Bohrdatei

**Datei → Fertigungsdateien → Bohrdatei** im Excellon-Format exportieren.

### Fertiger

Den `gerber/`-Ordner zippen und hochladen:

| Fertiger | Preis (5 Stück) |
|----------|-----------------|
| JLCPCB | ~$2 + Versand |
| PCBWay | ~$5 + Versand |
| Aisler | ~€15 (EU) |

---

## Kritische Verbindungen

Diese Verdrahtungen entscheiden über Funktion oder Fehlfunktion:

| Verbindung | Begründung |
|------------|------------|
| U2/U3 Pin 10 → VCC | SRCLR deaktiviert – verhindert ungewolltes Löschen |
| U2/U3 Pin 13 → GND | OE aktiviert Ausgänge |
| U5 Pin 11 → VCC | SER-Eingang darf bei CMOS nicht floaten |
| C1–C4 nah an ICs | Filtert Schaltspitzen |

---

*Stand: Dezember 2025 | KiCad 8.x | macOS*
