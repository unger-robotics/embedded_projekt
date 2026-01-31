# KiCad 8 Kurzanleitung

**Schaltplan-Editor – Tastenkürzel & Workflows**

Version 1.0 | Dezember 2025

---

## 1. Tastenkürzel im Schaltplan-Editor

### 1.1 Bauteile & Symbole

| Taste | Funktion |
|-------|----------|
| **A** | Symbol hinzufügen (Add Symbol) |
| **P** | Power-Symbol hinzufügen (VCC, GND, 3V3) |
| **M** | Verschieben (Move) |
| **R** | Rotieren (Rotate) |
| **E** | Eigenschaften bearbeiten (Edit Properties) |
| **G** | Ziehen (Drag) – behält Verbindungen |
| **Ctrl+C / Ctrl+V** | Kopieren / Einfügen |
| **Ctrl+D** | Duplizieren (mit Versatz) |
| **Del / Backspace** | Löschen |

### 1.2 Verbindungen & Labels

| Taste | Funktion |
|-------|----------|
| **W** | Draht zeichnen (Wire) |
| **L** | Label setzen (gleiche Namen = verbunden) |
| **Q** oder **X** | No-Connect setzen (für ungenutzte Pins) |
| **Esc** | Aktion abbrechen / Werkzeug beenden |

### 1.3 Navigation & Ansicht

| Taste/Aktion | Funktion |
|--------------|----------|
| **Mausrad drehen** | Zoomen |
| **Mausrad gedrückt** | Arbeitsfläche verschieben (Pan) |
| **Rechte Maustaste** | Kontextmenü / Pan |
| **Home** | Gesamtansicht (Fit to Screen) |
| **F** | Auf Auswahl zoomen (Fit Selection) |

---

## 2. Wichtige Workflows

### 2.1 Bauteile schnell platzieren

**Mehrfach platzieren (ohne Esc):**

1. `A` drücken → Symbol suchen (z.B. "R" für Widerstand)
2. Klicken zum Platzieren – Symbol bleibt am Cursor
3. Weiter klicken für weitere Bauteile
4. `Esc` erst am Ende drücken

**Kopieren für identische Werte:**

1. Ein Bauteil platzieren und Wert setzen (`E` → Value)
2. Bauteil auswählen → `Ctrl+C` → `Ctrl+V` (wiederholen)
3. Oder: `Ctrl+D` für Duplikat mit Versatz

### 2.2 Bauteile ausrichten

1. Mehrere Bauteile auswählen (Rechteck ziehen oder `Shift+Klick`)
2. Rechtsklick → **Auswahl transformieren**
   - Horizontal ausrichten (gleiche Y-Position)
   - Vertikal ausrichten (gleiche X-Position)
   - Horizontal/Vertikal verteilen (gleichmäßiger Abstand)

*Alternativ: "Elemente am Raster ausrichten" für grobes Ausrichten*

### 2.3 Verbindungen mit Labels

Labels verbinden automatisch alle Punkte mit gleichem Namen:

1. `L` drücken → Label-Name eingeben (z.B. "LED_DATA")
2. An Pin platzieren
3. Gleiches Label an anderem Pin → automatisch verbunden

**Vorteil:** Keine langen Drähte nötig, übersichtlicherer Schaltplan

### 2.4 Power-Symbole richtig verwenden

KiCad braucht eine "Quelle" für Spannungen:

1. `P` drücken → "PWR_FLAG" suchen
2. Je ein PWR_FLAG an VCC/3V3-Netz und GND-Netz platzieren
3. Mit Draht (`W`) an die Leitung anschließen

*Ohne PWR_FLAG: ERC meldet "Input-Power-Pin wird von keinem Output-Power-Pin angesteuert"*

---

## 3. ERC (Electrical Rules Checker)

### 3.1 ERC ausführen

- **Menü:** Prüfen → Electrical Rules Checker
- **Oder:** Klick auf das Käfer-Symbol in der Toolbar

### 3.2 Häufige ERC-Fehler

| Fehler | Lösung |
|--------|--------|
| Pin nicht verbunden | Draht anschließen oder No-Connect (`X`) setzen |
| Input-Power-Pin nicht angesteuert | PWR_FLAG an die Leitung setzen |
| Input-Pin nicht angesteuert | Pin mit Ausgang verbinden oder No-Connect |
| Multiple net names | Doppelte Labels entfernen |

### 3.3 No-Connect richtig setzen

Für ungenutzte Pins (z.B. ungenutzte IC-Ausgänge):

- `Q` oder `X` drücken
- Auf den unbenutzten Pin klicken
- Ein kleines X erscheint am Pin

**Wichtig:** CMOS-Eingänge im echten Aufbau trotzdem auf GND/VCC legen!

---

## 4. Wichtige Menüfunktionen

### 4.1 Werkzeuge-Menü

| Funktion | Beschreibung |
|----------|--------------|
| Schaltplansymbole annotieren | R? → R1, R2, R3... automatisch vergeben |
| Footprints zuordnen | THT/SMD-Gehäuse für Bauteile auswählen |
| Symbole aktualisieren | Symbole aus Bibliothek neu laden |
| PCB aus Schaltplan aktualisieren (`F8`) | Änderungen zum PCB-Editor übertragen |

### 4.2 Prüfen-Menü

| Funktion | Beschreibung |
|----------|--------------|
| Electrical Rules Checker (ERC) | Verbindungsfehler finden |
| Symbol auf Leiterplatte auswählen | Bauteil im PCB-Editor markieren |

---

## 5. Schnelle Symbol-Suche

Nach Drücken von `A` die folgenden Suchbegriffe eingeben:

| Suchbegriff | Bauteil | Bibliothek |
|-------------|---------|------------|
| R | Widerstand | Device |
| C | Kondensator | Device |
| LED | Leuchtdiode | Device |
| SW_Push | Taster | Switch |
| 74HC595 | Schieberegister (Output) | 74xx |
| 4021 | Schieberegister (Input) | 4xxx |
| Conn_01x14 | 14-Pin Stecker (ESP32) | Connector |

---

## 6. Typischer Workflow

### Schaltplan erstellen

1. **Projekt erstellen:** Datei → Neues Projekt
2. **ICs platzieren:** `A` → Suchen → Platzieren
3. **Passive Bauteile:** R, C, LED, SW_Push
4. **Werte setzen:** `E` → Value ändern (z.B. 330, 10k, 100n)
5. **Power:** `P` → VCC/3V3, GND, PWR_FLAG
6. **Verbinden:** `W` für Drähte, `L` für Labels
7. **No-Connect:** `X`/`Q` für ungenutzte Pins
8. **Prüfen:** ERC → 0 Fehler anstreben
9. **Annotieren:** Werkzeuge → Schaltplansymbole annotieren
10. **Footprints:** Werkzeuge → Footprints zuordnen
11. **PCB:** `F8` → Update PCB from Schematic

---

## 7. Footprint-Referenz (THT)

| Bauteil | Footprint |
|---------|-----------|
| 74HC595, CD4021 | Package_DIP:DIP-16_W7.62mm |
| Widerstand | Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal |
| LED 5mm | LED_THT:LED_D5.0mm |
| Kondensator | Capacitor_THT:C_Disc_D5.0mm_W2.5mm_P2.50mm |
| Taster 6mm | Button_Switch_THT:SW_PUSH_6mm |

---

*Erstellt für Selection Panel Prototyp | KiCad 8.0*
