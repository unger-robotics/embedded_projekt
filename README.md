# Selection-Panel

Browser-Dashboard ↔ WebSocket(JSON) ↔ Bridge(Python) ↔ Serial(USB) ↔ ESP32 (One-Hot Selection)

## Übersicht

Ein 10-Kanal Selection-Panel mit Hardware-Buttons, LED-Feedback und Web-Interface. Nur ein Kanal kann gleichzeitig aktiv sein (One-Hot).

| Komponente        | Technologie                           | Pfad              |
|-------------------|---------------------------------------|-------------------|
| **Firmware**      | ESP32-S3, PlatformIO, Arduino         | `firmware/`       |
| **Bridge**        | Python 3.10+, aiohttp, async          | `apps/bridge/`    |
| **Dashboard**     | React 18, TypeScript, Tailwind        | `apps/dashboard/` |
| **Hardware**      | KiCad 8, CD4021/HC595 Schieberegister | `hardware/kicad/` |
| **Dokumentation** | Markdown (SSOT), LaTeX                | `doc/`            |

## Projektstruktur

```
├── firmware/           ESP32-S3 Firmware (PlatformIO)
│   ├── src/app/        Applikationsschicht
│   ├── src/logic/      Geschäftslogik (hardware-unabhängig)
│   ├── src/drivers/    Chip-Treiber (CD4021, HC595)
│   └── src/protocol/   Serial-Protokoll (COBS/CRC)
├── apps/
│   ├── bridge/         Python WebSocket ↔ Serial Bridge
│   └── dashboard/      React SPA (Vite)
├── hardware/kicad/     Schaltplan + PCB
├── doc/
│   ├── md/             Markdown-Dokumentation (SSOT)
│   └── latex/          LaTeX/PDF-Ausgabe
└── scripts/            Deployment- und Utility-Skripte
```

## Quick Start

### Firmware flashen
```bash
cd firmware
pio run -t upload && pio device monitor
```

### Dashboard + Bridge lokal starten
```bash
# Terminal 1: Dashboard Dev-Server
cd apps/dashboard && npm ci && npm run dev

# Terminal 2: Bridge (verbindet mit ESP32)
cd apps/bridge
python -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
python bridge.py --port auto --http 8081
```

Dashboard öffnen: http://localhost:5173 (Dev) oder http://rover.local:8081/ (via Bridge)

## Dokumentation

| Thema                 | Pfad                                                |
|-----------------------|-----------------------------------------------------|
| **Claude Code Guide** | `CLAUDE.md`                                         |
| **Protokoll (SSOT)**  | `apps/bridge/docs/PROTOCOL.md`                      |
| **Build/Deploy/Run**  | `apps/bridge/docs/WORKFLOW.md`                      |
| **Smoke Test (E2E)**  | `apps/bridge/docs/SMOKE_TEST.md`                    |
| **Firmware Details**  | `firmware/CLAUDE.md`, `firmware/docs/`              |
| **Apps Details**      | `apps/CLAUDE.md`                                    |
| **Hardware (KiCad)**  | `hardware/kicad/selection-panel-prototyp/README.md` |

## Deployment (Pi)

Siehe `CLAUDE.md` für den vollständigen Deploy-Workflow (Mac → Pi) und systemd-Konfiguration.

```bash
# Kurzfassung: Dashboard bauen und zum Pi synchronisieren
cd apps/dashboard && npm run build && cd -
./scripts/deploy_pi.sh
```

Auf dem Pi: `sudo systemctl restart embedded-bridge.service`

## Lizenz

Siehe `LICENSE`.
