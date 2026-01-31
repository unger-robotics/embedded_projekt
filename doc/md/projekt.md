# Selection-Panel-Projekt: Gesamtübersicht


## 0) Begriffsrahmen und Zielsetzung

### Anwendungskontext

Das Selection-Panel-Projekt realisiert ein interaktives Hardware-Interface mit 10–100 Buttons und LEDs. Zu jedem Zeitpunkt ist höchstens eine Auswahl aktiv (One-Hot-Prinzip). Das System verbindet drei Ebenen: Embedded-Firmware (ESP32-S3), eine Bridge (Raspberry Pi) und ein Web-Dashboard (Browser). Diese Dokumentation gibt einen strukturierten Überblick über Architektur, Komponenten und Arbeitsabläufe.

**Projektroot:** `/Users/jan/daten/IoT/embedded_projekt`

### Lernziele

Nach dem Durcharbeiten dieses Dokuments kannst du:

1. **Die Systemarchitektur erklären** – drei Ebenen, ihre Verantwortlichkeiten und Schnittstellen benennen
2. **Den Datenfluss nachvollziehen** – den Weg vom Button-Druck bis zur Browser-Anzeige tracen
3. **Die Repository-Struktur navigieren** – wissen, wo Code, Dokumentation und Konfiguration liegen
4. **Das SSOT-Prinzip anwenden** – verstehen, welche Datei für welche Konfiguration maßgeblich ist
5. **Die Entwicklungsworkflows ausführen** – Firmware flashen, Dashboard bauen, auf Pi deployen
6. **Teststrategien anwenden** – Unit-Tests und E2E-Tests unterscheiden und ausführen

### Begriffsdefinitionen

| Begriff             | Definition                                                                   | Quelle/Kontext        |
|---------------------|------------------------------------------------------------------------------|-----------------------|
| **Selection Panel** | Hardware-Interface mit Tastern und LEDs für Kanalauswahl                     | Projektspezifisch     |
| **One-Hot**         | Invariante: genau 0 oder 1 Auswahl aktiv, niemals mehrere gleichzeitig       | Digital-Design        |
| **CD4021B**         | 8-Bit Parallel-zu-Seriell Schieberegister für Button-Eingabe                 | TI Datasheet          |
| **74HC595**         | 8-Bit Seriell-zu-Parallel Schieberegister für LED-Ausgabe                    | NXP Datasheet         |
| **Bridge**          | Python-Server, der WebSocket ↔ Serial-Protokoll übersetzt                    | Projektspezifisch     |
| **SSOT**            | Single Source of Truth – jede Konfiguration existiert an genau einer Stelle  | Design-Patterns       |
| **USB-CDC**         | USB Communication Device Class – virtueller Serial-Port über USB             | USB-IF CDC 1.2        |
| **COBS**            | Consistent Overhead Byte Stuffing – Framing-Verfahren mit 0x00 als Delimiter | Cheshire/Baker (1999) |
| **FreeRTOS**        | Echtzeit-Betriebssystem für Embedded-Systeme                                 | freertos.org          |

---

## 1) Systemarchitektur – Drei Ebenen

### Problemstellung

Ein interaktives Panel braucht mehrere Fähigkeiten: Hardware-Steuerung (schnelle IO-Reaktion), Netzwerk-Kommunikation (Dashboard-Anbindung) und Benutzeroberfläche (visuelle Darstellung). Diese Anforderungen haben unterschiedliche Charakteristiken und eignen sich für unterschiedliche Technologien.

### Regel

Die Architektur trennt Verantwortlichkeiten auf drei Ebenen. Jede Ebene hat eine klare Aufgabe und kommuniziert über definierte Schnittstellen mit den Nachbarebenen.

### Herleitung: Die drei Ebenen

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          BROWSER (Dashboard)                            │
│  Technologie: React + TypeScript + Vite + Tailwind                      │
│  Aufgabe:     Visualisierung, Benutzerinteraktion                       │
│  Quellcode:   apps/dashboard/src/                                       │
└───────────────────────────────┬─────────────────────────────────────────┘
                                │
                          WebSocket (JSON)
                          ws://rover.local:8081/ws
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                       RASPBERRY PI (Bridge)                             │
│  Technologie: Python + aiohttp (async)                                  │
│  Aufgabe:     Protokoll-Übersetzung, HTTP-Server, Logging               │
│  Runtime:     apps/bridge/bridge.py                                     │
│  Static:      apps/dashboard/dist/ (wird von Bridge ausgeliefert)       │
└───────────────────────────────┬─────────────────────────────────────────┘
                                │
                          USB-CDC Serial
                          COBS + CRC16, 921600 Baud
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                        ESP32-S3 (Firmware)                              │
│  Technologie: C++ + FreeRTOS                                            │
│  Aufgabe:     Echtzeit-IO, Debounce, One-Hot-Logik                      │
│  Quellcode:   firmware/src/                                             │
│  Hardware:    CD4021B (Buttons) + 74HC595 (LEDs) via SPI                │
└─────────────────────────────────────────────────────────────────────────┘
```

### Anwendung: Warum diese Aufteilung?

| Ebene    | Stärke                               | Schwäche                          |
|----------|--------------------------------------|-----------------------------------|
| ESP32-S3 | Echtzeit-IO, µs-Timing, Hardware-nah | Kein Netzwerk-Stack, wenig RAM    |
| Pi       | Netzwerk, Logging, Dateisystem       | Kein Echtzeit-OS, Latenz variabel |
| Browser  | UI-Flexibilität, plattformunabhängig | Keine Hardware-Kontrolle          |

Jede Ebene macht das, was sie am besten kann. Der ESP32 reagiert in Mikrosekunden auf Buttons – dafür ist er optimiert. Der Browser rendert ein reaktives UI – dafür ist er optimiert. Der Pi verbindet beide Welten.

### Schnittstellen-Spezifikation

| Schnittstelle    | Protokoll                  | Bandbreite  | Latenz (typisch) |
|------------------|----------------------------|-------------|------------------|
| ESP32 ↔ Pi       | USB-CDC Serial, COBS/CRC16 | 921600 Baud | 1–5 ms           |
| Pi ↔ Browser     | WebSocket, JSON            | 10+ Mbit/s  | 5–20 ms          |
| Gesamt Button→UI | –                          | –           | 40–75 ms         |

### Zusammenfassung

> **Merkregel:** „Drei Ebenen, drei Sprachen, drei Aufgaben. ESP32 = Hardware, Pi = Netzwerk, Browser = UI."

---

## 2) Datenfluss – Vom Button zum Browser

### Problemstellung

Wenn ein Benutzer Button 7 drückt, muss diese Information durch alle drei Ebenen fließen und am Ende im Browser sichtbar werden. Wie genau funktioniert dieser Fluss?

### Herleitung: Schritt-für-Schritt

```
┌─────────────────────────────────────────────────────────────────────────┐
│ 1. HARDWARE: CD4021B liest Button-Matrix                                │
│    Schieberegister → SPI-Transfer → rawButtons[BTN_BYTES]               │
│    Frequenz: alle 10 ms (IO-Task)                                       │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│ 2. FIRMWARE: Debounce + Selection-Logik                                 │
│    rawButtons → Debounce (50 ms) → Press-Edge erkannt                   │
│    Selection-Policy: Toggle/Radio → selectedId = 7                      │
│    One-Hot-Invariante: setOneHot(7) → ledState[]                        │
│    74HC595 schreibt LED-Zustand                                         │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│ 3. FIRMWARE → SERIAL: SELECTION_STATE-Paket                             │
│    Header: magic=SP, type=0x12, seq=1234, len=8                         │
│    Payload: selected_id=7, source=local_btn, t_ms=45678                 │
│    CRC-16 anhängen → COBS-encode → 0x00-Delimiter → USB-CDC senden      │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                              USB-CDC Serial
                              921600 Baud
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│ 4. BRIDGE: Frame empfangen und übersetzen                               │
│    Bytes sammeln bis 0x00 → COBS-decode → CRC prüfen                    │
│    Header parsen → type=SELECTION_STATE erkannt                         │
│    ACK senden (seq=1234)                                                │
│    JSON erzeugen: {"type":"selection","selected":7,"source":"local_btn"}│
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                              WebSocket
                              ws://rover.local:8081/ws
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│ 5. BROWSER: React State-Update                                          │
│    useBridgeWs() Hook empfängt JSON                                     │
│    setSelectedChannel(7) → Re-Render                                    │
│    UI zeigt: LED 7 leuchtet grün, andere aus                            │
└─────────────────────────────────────────────────────────────────────────┘
```

### Timing-Analyse

| Segment                     | Dauer (typisch) | Einflussfaktoren                |
|-----------------------------|-----------------|---------------------------------|
| Button → Debounce-Erkennung | 30–50 ms        | Debounce-Schwellwert (3× 10 ms) |
| Firmware-Verarbeitung       | < 1 ms          | Logik, SPI-Transfer             |
| Serial-Übertragung          | 1–5 ms          | USB-CDC, Pufferung              |
| Bridge-Verarbeitung         | < 1 ms          | Python async                    |
| WebSocket → Browser         | 5–20 ms         | Netzwerk, Browser-Event-Loop    |
| **Gesamt**                  | **40–75 ms**    |                                 |

### Rückrichtung: Dashboard → LED

Der umgekehrte Fluss (Benutzer klickt im Dashboard, LED leuchtet) folgt demselben Muster in umgekehrter Richtung:

```
Browser                    Pi                         ESP32
   │                        │                           │
   │── WS: {cmd:select,     │                           │
   │        channel:7} ───► │                           │
   │                        │── CMD_SET_SELECTION ────► │  seq=99, ACK_REQ
   │                        │      (COBS/CRC)           │
   │                        │◄──────── ACK ──────────── │  seq=99
   │                        │                           │  (setzt LED 7)
   │                        │◄── SELECTION_STATE ────── │  source=host_cmd
   │◄── WS: {type:selection,│                           │
   │         selected:7} ───│                           │
```

### Zusammenfassung

> **Merkregel:** „Fünf Stufen: Hardware → Firmware → Serial → Bridge → Browser. Jede Stufe transformiert das Format."

---

## 3) Repository-Struktur

### Problemstellung

Bei einem Projekt mit Firmware, Backend und Frontend in einem Repository ist eine klare Struktur essentiell. Wo liegt was? Wie finde ich die richtige Datei?

### Herleitung: Verzeichnisbaum

```
embedded_projekt/
│
├── CLAUDE.md                          # Build-Befehle, Architektur (für Claude Code)
├── README.md                          # Projekt-Übersicht, Quick Start
│
├── apps/                              # Anwendungen (nicht Firmware)
│   │
│   ├── bridge/                        # Python Bridge (läuft auf Pi)
│   │   ├── bridge.py                 # Hauptprogramm: HTTP + WS + Serial
│   │   ├── requirements.txt          # Python-Abhängigkeiten
│   │   └── docs/
│   │       ├── PROTOCOL.md           # Serial + WS Protokoll (SSOT!)
│   │       ├── WORKFLOW.md           # Build/Deploy/Run
│   │       └── SMOKE_TEST.md         # E2E-Testanleitung
│   │
│   └── dashboard/                     # React Frontend (läuft im Browser)
│       ├── src/
│       │   ├── pages/                # Seiten: DashboardPage, SettingsPage
│       │   ├── components/           # UI-Komponenten: Button, LED, etc.
│       │   └── lib/                  # Utility: protocol.ts, ws.ts
│       ├── dist/                     # Build-Output (wird auf Pi deployed)
│       ├── docs/
│       │   ├── DEVELOPER.md          # Frontend-Entwicklung
│       │   └── USER_GUIDE.md         # Benutzerhandbuch
│       ├── vite.config.ts            # Build-Konfiguration
│       └── package.json              # npm-Abhängigkeiten
│
├── firmware/                          # ESP32-S3 Firmware (C++)
│   ├── include/
│   │   └── hw/                       # SSOT: Hardware-Konfiguration
│   │       ├── board_pins.h          # Pin-Definitionen
│   │       ├── io_counts.h           # Counts, Bytes, Masks
│   │       └── spi_params.h          # SPI-Frequenz, Mode
│   ├── src/
│   │   ├── hal/                      # Hardware Abstraction Layer
│   │   ├── drivers/                  # IC-Treiber: CD4021, HC595
│   │   ├── logic/                    # Debounce, Selection, Pipeline
│   │   ├── app/                      # Task/Loop Orchestrierung
│   │   └── protocol/                 # COBS/CRC Frame-Handling
│   ├── test/                         # Unit + E2E Tests
│   ├── docs/                         # Firmware-Dokumentation
│   └── platformio.ini                # Build-Konfiguration
│
├── hardware/
│   └── kicad/                         # Schaltpläne, PCB-Layout
│
├── doc/
│   ├── md/                            # Markdown-Quellen (SSOT)
│   └── latex/                         # LaTeX für PDF-Generierung
│
├── assets/
│   ├── pdf/                           # Generierte PDFs
│   └── video/                         # Demo-Videos, Medien
│
└── scripts/                           # Build-/Deploy-Helfer
```

### Anwendung: Schnelle Navigation

| „Ich suche..."                 | Pfad                               |
|--------------------------------|------------------------------------|
| Pin-Definitionen               | `firmware/include/hw/board_pins.h` |
| Serial-Protokoll-Spezifikation | `apps/bridge/docs/PROTOCOL.md`     |
| React-Komponenten              | `apps/dashboard/src/components/`   |
| Unit-Tests für Debounce        | `firmware/test/`                   |
| Schaltplan                     | `hardware/kicad/`                  |
| Bridge-Startskript             | `apps/bridge/bridge.py`            |

### Zusammenfassung

> **Merkregel:** „apps/ = Pi + Browser, firmware/ = ESP32, doc/ = Dokumentation. SSOT lebt in include/hw/ und PROTOCOL.md."

---

## 4) SSOT – Single Source of Truth

### Problemstellung

Wenn dieselbe Information (z. B. Pin-Nummer, Protokoll-Konstante) an mehreren Stellen definiert ist, entstehen Inkonsistenzen. Eine Änderung wird vergessen, das System verhält sich unerklärlich.

### Regel

Jede Konfiguration existiert an genau einer Stelle. Alle anderen Codestellen referenzieren diese Quelle. Änderungen propagieren automatisch.

### Herleitung: SSOT-Zuordnung

| Aspekt              | SSOT-Datei                         | Konsumenten                         |
|---------------------|------------------------------------|-------------------------------------|
| GPIO-Pins           | `firmware/include/hw/board_pins.h` | hal/, drivers/                      |
| Kanalanzahl, Masken | `firmware/include/hw/io_counts.h`  | logic/, drivers/, tests             |
| SPI-Parameter       | `firmware/include/hw/spi_params.h` | hal/spi_bus.cpp                     |
| Serial-Protokoll    | `apps/bridge/docs/PROTOCOL.md`     | firmware/protocol/, bridge.py, lib/ |
| WebSocket-Events    | `apps/bridge/docs/PROTOCOL.md`     | bridge.py, dashboard/src/lib/       |

### Beispiel: io_counts.h als SSOT

```c
// firmware/include/hw/io_counts.h – SSOT für Kanalanzahl

#define BTN_COUNT       10
#define LED_COUNT       10

#define BTN_BYTES       ((BTN_COUNT + 7) / 8)  // = 2
#define LED_BYTES       ((LED_COUNT + 7) / 8)  // = 2

#define LED_LAST_MASK   0xC0  // Gültige Bits im letzten Byte
```

**Änderung von 10 auf 100 Kanäle:**

1. Ändere `BTN_COUNT` und `LED_COUNT` in `io_counts.h`
2. `BTN_BYTES` wird automatisch 13 (statt 2)
3. `LED_LAST_MASK` muss manuell angepasst werden (0xF0 für 100)
4. Alle anderen Dateien referenzieren diese Makros – keine weitere Änderung nötig

### Generierte Artefakte (keine SSOT)

Diese Dateien werden aus Quellen generiert und dürfen nicht manuell editiert werden:

| Artefakt               | Quelle                | Generator       |
|------------------------|-----------------------|-----------------|
| `apps/dashboard/dist/` | `apps/dashboard/src/` | `npm run build` |
| `assets/pdf/*.pdf`     | `doc/latex/*.tex`     | LaTeX           |
| `.pio/build/`          | `firmware/src/`       | PlatformIO      |

### Zusammenfassung

> **Merkregel:** „Eine Zahl, eine Stelle. Wer kopiert, verliert. Wer generiert, referenziert."

---

## 5) Firmware – Build, Flash, Test

### Problemstellung

Die Firmware muss auf den ESP32-S3 geflasht werden. Verschiedene Varianten (FreeRTOS vs. Arduino-Loop) und Testmodi erfordern unterschiedliche Build-Konfigurationen.

### Build-Environments

| Environment                   | Beschreibung                       | Verwendung               |
|-------------------------------|------------------------------------|--------------------------|
| `seeed_xiao_esp32s3_freertos` | FreeRTOS-Task (APP_USE_FREERTOS=1) | Produktion               |
| `seeed_xiao_esp32s3_loop`     | Arduino-Loop (APP_USE_FREERTOS=0)  | Einfache Tests           |
| `seeed_xiao_esp32s3_test`     | Unit-Tests (nur logic/)            | CI, Entwicklung          |
| `seeed_xiao_esp32s3_e2e`      | E2E-Hardware-Test                  | Nach Hardware-Änderungen |

### Herleitung: Typische Befehle

**Flashen (FreeRTOS-Variante):**

```bash
cd /Users/jan/daten/IoT/embedded_projekt

# Build + Upload
pio run -d firmware -e seeed_xiao_esp32s3_freertos -t upload
```

**Serial-Monitor:**

```bash
pio device monitor -d firmware -e seeed_xiao_esp32s3_freertos
```

**Unit-Tests (Logik):**

```bash
# Testet: Debounce, Selection, Pipeline – ohne Hardware
pio test -d firmware -e seeed_xiao_esp32s3_test -v
```

**E2E-Hardware-Tests:**

```bash
# Testet: SPI, Pinning, Timing – braucht echte Hardware
pio test -d firmware -e seeed_xiao_esp32s3_e2e -f test_e2e_hw -v
```

### Anwendung: Wann welcher Test?

| Änderung                       | Erforderlicher Test                    |
|--------------------------------|----------------------------------------|
| Code in `logic/`               | Unit-Tests (`seeed_xiao_esp32s3_test`) |
| Code in `drivers/` oder `hal/` | E2E-Test + Unit-Tests                  |
| Pins/Verdrahtung               | E2E-Test + `HARDWARE.md` aktualisieren   |
| Counts (10 → 100)              | Unit-Tests + E2E + Timing-Prüfung      |

### Zusammenfassung

> **Merkregel:** „pio test für Logik, pio run -t upload für Hardware. Unit-Tests vor E2E."

---

## 6) Dashboard – Entwicklung und Build

### Problemstellung

Das React-Frontend hat zwei Modi: Entwicklung (schnelles Feedback mit Hot-Reload) und Produktion (optimierter Build für Deployment). Wie wechselt man zwischen beiden?

### Entwicklungsmodus (DEV)

```bash
cd /Users/jan/daten/IoT/embedded_projekt/apps/dashboard

# Abhängigkeiten installieren (einmalig oder nach package.json-Änderung)
npm ci

# DEV-Server starten
npm run dev
```

**Ergebnis:**
- Lokaler Server: `http://localhost:5173/`
- Hot Module Replacement (HMR): Änderungen erscheinen sofort
- WebSocket-Proxy: Anfragen an `/ws` werden zu `rover.local:8081` weitergeleitet

### Produktions-Build

```bash
# Lint-Prüfung (Code-Qualität)
npm run lint

# TypeScript-Prüfung (Typ-Sicherheit)
npm run typecheck

# Optimierter Build
npm run build
```

**Ergebnis:**
- Output in `dist/` (minifiziert, tree-shaken)
- Bereit für Deployment auf Pi

### npm-Scripts Übersicht

| Script              | Zweck                       | Wann verwenden?         |
|---------------------|-----------------------------|-------------------------|
| `npm run dev`       | DEV-Server mit HMR          | Während der Entwicklung |
| `npm run lint`      | ESLint-Prüfung              | Vor Commit/Build        |
| `npm run typecheck` | TypeScript-Prüfung          | Vor Commit/Build        |
| `npm run build`     | Production-Build nach dist/ | Vor Deployment          |
| `npm run format`    | Prettier-Formatierung       | Bei Bedarf              |

### Zusammenfassung

> **Merkregel:** „npm run dev zum Entwickeln, npm run build vor Deploy. Immer lint + typecheck vor build."

---

## 7) Bridge – Raspberry Pi

### Problemstellung

Die Bridge ist das Bindeglied zwischen ESP32 und Browser. Sie muss Serial lesen, WebSocket bedienen und HTTP für das statische Dashboard ausliefern – alles gleichzeitig und asynchron.

### Manueller Start (Debugging)

```bash
# SSH zum Pi
ssh pi@rover

# Python-Umgebung aktivieren
source ~/venvs/selection-bridge/bin/activate

# Bridge starten mit allen Parametern
python /home/pi/embedded_projekt/apps/bridge/bridge.py \
  --port auto \
  --vid 0x303A --pid 0x1001 \
  --baud 921600 \
  --http 8081 \
  --web-root /home/pi/embedded_projekt/apps/dashboard/dist \
  --wait-port --wait-timeout 0 --wait-poll 0.5 \
  --reconnect-delay 2 \
  --ack-timeout 0.6 --retries 4
```

### Parameter-Erklärung

| Parameter           | Wert      | Bedeutung                               |
|---------------------|-----------|-----------------------------------------|
| `--port auto`       | –         | Automatische Port-Erkennung via VID/PID |
| `--vid 0x303A`      | Espressif | USB Vendor ID                           |
| `--pid 0x1001`      | ESP32-S3  | USB Product ID                          |
| `--baud 921600`     | –         | Serial-Baudrate                         |
| `--http 8081`       | –         | HTTP/WebSocket-Port                     |
| `--web-root ...`    | dist/     | Statische Dateien für Dashboard         |
| `--ack-timeout 0.6` | 600 ms    | Wartezeit auf ACK                       |
| `--retries 4`       | –         | Wiederholungsversuche bei Timeout       |

### systemd-Service (Produktion)

```bash
# Status prüfen
sudo systemctl status embedded-bridge.service --no-pager -l

# Neu starten (nach Code-Änderung)
sudo systemctl restart embedded-bridge.service

# Logs live verfolgen
sudo journalctl -u embedded-bridge.service -f

# Logs der letzten Stunde
sudo journalctl -u embedded-bridge.service --since "1 hour ago"
```

### URLs (Produktion)

| Zweck        | URL                                                    |
|--------------|--------------------------------------------------------|
| Dashboard    | `http://rover.local:8081/`                             |
| WebSocket    | `ws://rover.local:8081/ws`                             |
| Health-Check | `http://rover.local:8081/health` (falls implementiert) |

### Zusammenfassung

> **Merkregel:** „Manuell starten zum Debuggen, systemctl für Produktion. Logs immer mit journalctl."

---

## 8) Deployment-Workflow

### Problemstellung

Code wird auf dem Mac entwickelt, muss aber auf dem Pi laufen. Wie synchronisiert man effizient, ohne unnötige Dateien zu übertragen?

### Schritt 1: Quellcode synchronisieren (Mac → Pi)

```bash
cd /Users/jan/daten/IoT/embedded_projekt

# Zielpfad anlegen (einmalig)
ssh pi@rover 'mkdir -p /home/pi/embedded_projekt/apps/bridge /home/pi/embedded_projekt/apps/dashboard'

# Sync ohne generierte/temporäre Dateien
rsync -avh --delete \
  --exclude '.git/' \
  --exclude '.venv/' \
  --exclude '__pycache__/' \
  --exclude '*.pyc' \
  --exclude '.DS_Store' \
  --exclude 'node_modules/' \
  --exclude 'apps/dashboard/dist/' \
  ./ \
  pi@rover:/home/pi/embedded_projekt/
```

**Flags erklärt:**

| Flag       | Bedeutung                                        |
|------------|--------------------------------------------------|
| `-a`       | Archiv-Modus (rekursiv, Permissions erhalten)    |
| `-v`       | Verbose (zeigt übertragene Dateien)              |
| `-h`       | Human-readable Größen                            |
| `--delete` | Entfernt Dateien auf Ziel, die auf Quelle fehlen |

### Schritt 2: Dashboard bauen und deployen

```bash
# Lokal bauen
cd apps/dashboard
npm ci
npm run lint && npm run typecheck && npm run build
cd -

# Nur dist/ synchronisieren
rsync -avh --delete \
  --exclude '.DS_Store' \
  ./apps/dashboard/dist/ \
  pi@rover:/home/pi/embedded_projekt/apps/dashboard/dist/
```

### Schritt 3: Bridge neu starten

```bash
ssh pi@rover "sudo systemctl daemon-reload && sudo systemctl restart embedded-bridge.service"
```

### Kompletter Deployment-Ablauf (Skript)

```bash
#!/bin/bash
# deploy.sh – Komplettes Deployment

set -e  # Bei Fehler abbrechen

echo "=== 1. Dashboard bauen ==="
cd apps/dashboard
npm ci
npm run lint
npm run typecheck
npm run build
cd -

echo "=== 2. Quellcode sync ==="
rsync -avh --delete \
  --exclude '.git/' --exclude '.venv/' --exclude '__pycache__/' \
  --exclude '*.pyc' --exclude '.DS_Store' --exclude 'node_modules/' \
  --exclude 'apps/dashboard/dist/' \
  ./ pi@rover:/home/pi/embedded_projekt/

echo "=== 3. Dashboard dist sync ==="
rsync -avh --delete \
  --exclude '.DS_Store' \
  ./apps/dashboard/dist/ pi@rover:/home/pi/embedded_projekt/apps/dashboard/dist/

echo "=== 4. Service neu starten ==="
ssh pi@rover "sudo systemctl restart embedded-bridge.service"

echo "=== Deployment fertig ==="
```

### Zusammenfassung

> **Merkregel:** „rsync mit --exclude, npm run build vor dist-Sync, systemctl restart zum Schluss."

---

## 9) Dokumentationsstruktur

### Problemstellung

Dokumentation verteilt sich auf mehrere Orte. Wo finde ich die richtige Doku für mein Problem?

### Übersicht nach Komponente

**Firmware (`firmware/docs/`):**

| Datei               | Inhalt                                            |
|---------------------|---------------------------------------------------|
| `architecture.md`   | Schichtenmodell (HAL → Driver → Logic → App)      |
| `HARDWARE.md`       | Hardware-Verdrahtung (Pins, ICs, Steckerbelegung) |
| `ALGORITHM.md`      | Algorithmen (Debounce, Selection, Pipeline)       |
| `DEVELOPER.md`      | API-Dokumentation                                 |
| `CODING_STANDARD.md`| Coding-Richtlinien (BARR-C, MISRA)               |

**Bridge (`apps/bridge/docs/`):**

| Datei           | Inhalt                               |
|-----------------|--------------------------------------|
| `PROTOCOL.md`   | Serial + WebSocket Protokoll (SSOT!) |
| `WORKFLOW.md`   | Build/Deploy/Run Anleitungen         |
| `SMOKE_TEST.md` | E2E-Testanleitung für manuelle Tests |

**Dashboard (`apps/dashboard/docs/`):**

| Datei           | Inhalt                            |
|-----------------|-----------------------------------|
| `DEVELOPER.md`  | Frontend-Entwicklung, Architektur |
| `USER_GUIDE.md` | Benutzerhandbuch für Endanwender  |

### Anwendung: Welche Doku für welches Problem?

| Problem                                 | Dokumentation                      |
|-----------------------------------------|------------------------------------|
| „Welcher Pin ist der SPI-Clock?"        | `firmware/docs/HARDWARE.md`        |
| „Wie sieht ein SELECTION_STATE aus?"    | `apps/bridge/docs/PROTOCOL.md`     |
| „Wie starte ich den DEV-Server?"        | `apps/dashboard/docs/DEVELOPER.md` |
| „Wie flashe ich die Firmware?"          | `CLAUDE.md` oder diese Datei       |
| „Wie teste ich nach Hardware-Änderung?" | `firmware/docs/architecture.md`    |
| „Build-Befehle auf einen Blick?"        | `CLAUDE.md` (Root)                 |

### Zusammenfassung

> **Merkregel:** „PROTOCOL.md = SSOT für Kommunikation. HARDWARE.md = SSOT für Pinning. CLAUDE.md = Build-Befehle. Diese Datei = Konzepte."

---

## 10) Typische Arbeitsabläufe

### Workflow A: Firmware-Logik ändern

```bash
# 1. Code ändern
#    Bearbeite: firmware/src/logic/debounce.cpp (oder selection.cpp, etc.)

# 2. Unit-Tests
pio test -d firmware -e seeed_xiao_esp32s3_test -v

# 3. Bei Erfolg: Flashen
pio run -d firmware -e seeed_xiao_esp32s3_freertos -t upload

# 4. Manuell testen
pio device monitor -d firmware

# 5. Optional: E2E-Test
pio test -d firmware -e seeed_xiao_esp32s3_e2e -f test_e2e_hw -v
```

### Workflow B: Dashboard-UI ändern

```bash
# 1. DEV-Server starten
cd apps/dashboard && npm run dev

# 2. Code ändern (Browser aktualisiert automatisch via HMR)
#    Bearbeite: src/components/, src/pages/, etc.

# 3. Lint + TypeCheck + Build
npm run lint && npm run typecheck && npm run build

# 4. Deploy (siehe Abschnitt 8)
```

### Workflow C: Hardware-Verdrahtung ändern

```bash
# 1. SSOT aktualisieren
#    Bearbeite: firmware/include/hw/board_pins.h

# 2. Dokumentation aktualisieren
#    Bearbeite: firmware/docs/HARDWARE.md

# 3. E2E-Test
pio test -d firmware -e seeed_xiao_esp32s3_e2e -f test_e2e_hw -v

# 4. Flashen und manuell verifizieren
pio run -d firmware -e seeed_xiao_esp32s3_freertos -t upload
```

### Workflow D: Protokoll-Erweiterung (neuer Message-Type)

```bash
# 1. SSOT aktualisieren
#    Bearbeite: apps/bridge/docs/PROTOCOL.md

# 2. Firmware implementieren
#    Bearbeite: firmware/src/protocol/

# 3. Bridge implementieren
#    Bearbeite: apps/bridge/bridge.py

# 4. Dashboard implementieren (falls UI nötig)
#    Bearbeite: apps/dashboard/src/lib/protocol.ts

# 5. Testen: Unit → E2E → Manuell
```

### Zusammenfassung

> **Merkregel:** „SSOT zuerst, Tests danach. Nie Hardware ändern ohne HARDWARE.md-Update."

---

## 11) Roadmap und Status

### Aktueller Stand

| Milestone        | Status | Beschreibung                          |
|------------------|--------|---------------------------------------|
| 10× Buttons/LEDs | ✅      | Grundfunktion verifiziert             |
| Serial-Protokoll | ✅      | COBS/CRC, ACK/Retry implementiert     |
| React-Dashboard  | ✅      | DEV/PROD-Split, WebSocket-Integration |
| Bridge (Python)  | ✅      | Async Serial + HTTP + WebSocket       |
| systemd-Service  | ✅      | Autostart, Logging                    |

### Nächste Schritte

| Milestone         | Status | Beschreibung                              |
|-------------------|--------|-------------------------------------------|
| 100× Skalierung   | ⬜      | Counts/Masks + Kaskadierung + Timing-Test |
| Audio-Integration | ⬜      | Sound bei Selection-Change                |
| PCB-Design        | ⬜      | KiCad-Schaltplan → Fertigung              |
| OTA-Updates       | ⬜      | Firmware-Update über WiFi                 |

### Skalierungs-Checkliste (10 → 100)

- [ ] `BTN_COUNT` und `LED_COUNT` in `io_counts.h` anpassen
- [ ] `LED_LAST_MASK` neu berechnen
- [ ] Schieberegister-Kaskade verdrahten (13× CD4021, 13× 74HC595)
- [ ] SPI-Timing prüfen (längere Transfers)
- [ ] Unit-Tests ausführen
- [ ] E2E-Tests mit 100 Kanälen
- [ ] Timing-Budget validieren (< 10 ms pro Scan-Zyklus)

