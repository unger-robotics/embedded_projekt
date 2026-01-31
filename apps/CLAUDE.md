# CLAUDE.md

Diese Datei gibt Claude Code (claude.ai/code) Orientierung für die Arbeit in diesem Repository.

## Build-Befehle

### Dashboard (React SPA)

```bash
cd apps/dashboard
npm ci                    # Abhängigkeiten installieren
npm run dev               # Dev-Server: http://localhost:5173
npm run build             # Production-Build → dist/
```

### Bridge (Python)

```bash
cd apps/bridge
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python bridge.py --port auto --http 8081 --web-root ../dashboard/dist
```

Vollständiger Aufruf mit allen Optionen:
```bash
python bridge.py \
  --port auto --vid 0x303A --pid 0x1001 --baud 921600 \
  --http 8081 --web-root ../dashboard/dist \
  --wait-port --wait-timeout 0 --wait-poll 0.5 \
  --reconnect-delay 2 --ack-timeout 0.6 --retries 4
```

## Linting & Formatierung

### Dashboard

```bash
npm run lint              # ESLint
npm run format            # Prettier (schreiben)
npm run format:check      # Prettier (nur prüfen)
npm run typecheck         # TypeScript Typ-Prüfung
```

### Bridge

```bash
ruff check .              # Linting
ruff format .             # Formatierung
mypy .                    # Typ-Prüfung
```

## Architektur

```
Browser (React SPA)
  ↓ WebSocket (JSON)
Bridge (Python: aiohttp)
  ↓ Serial (COBS + CRC16, 921600 Baud)
ESP32 Firmware
```

### Bridge (`apps/bridge/`)

| Datei | Zweck |
|-------|-------|
| `bridge.py` | CLI-Einstiegspunkt, Argument-Parsing |
| `bridge_core.py` | Serial ↔ WebSocket State-Machine, Reconnect-Logik |
| `web_app.py` | aiohttp App-Setup, WebSocket-Handler, SPA-Routing |
| `protocol.py` | Nachrichtentypen, Paket-Aufbau/-Parsing |
| `cobs.py` | COBS Frame-Encoding/-Decoding |
| `crc.py` | CRC16 CCITT-FALSE |
| `serial_link.py` | Serial-Port Autoerkennung (USB VID/PID) |

### Dashboard (`apps/dashboard/`)

| Verzeichnis | Zweck |
|-------------|-------|
| `src/pages/` | Routen-Seiten (Dashboard, Logs, Settings) |
| `src/components/` | Wiederverwendbare UI (ButtonChips, LedGrid, StatusPill) |
| `src/lib/ws/` | WebSocket-Client, React Context Store, Nachricht-Codec |

Einstiegspunkt: `src/main.tsx` → `src/App.tsx` (React Router)

## Protokoll

**SSOT**: `apps/bridge/docs/PROTOCOL.md`

### Serial (ESP32 ↔ Bridge)
- COBS-gerahmt mit 0x00 Delimiter
- Paket: Header (10B) + Payload + CRC16
- Nachrichten: HELLO (0x01), SELECTION_STATE (0x12), CMD_SET_SELECTION (0x20), ACK (0x30)

### WebSocket (Browser ↔ Bridge)
- Endpoint: `/ws`
- Kommandos (→ Bridge): `setSelection`, `ping`
- Events (← Bridge): `hello`, `selection`, `serial`, `ack`, `warn`, `error`

## Wichtige Einschränkungen

- **One-Hot-Selektion**: Nur ein Button kann gleichzeitig ausgewählt sein (0 = keiner)
- **WS-URL muss zum UI-Host passen**: Bei UI auf `rover.local:8081` muss WS `ws://rover.local:8081/ws` sein
- **Port-Autoerkennung**: Nutzt USB VID 0x303A, PID 0x1001 (Espressif ESP32-S3)
- **SPA-Fallback-Routing**: Bridge liefert `index.html` für alle unbekannten Routen

## Deployment (Mac → Pi)

```bash
# Dashboard bauen
cd apps/dashboard && npm ci && npm run build && cd -

# Zum Pi synchronisieren (ohne venv/node_modules)
rsync -avh --delete \
  --exclude '.git/' --exclude '.venv/' --exclude '__pycache__/' \
  --exclude '*.pyc' --exclude '.DS_Store' --exclude 'node_modules/' \
  --exclude 'apps/dashboard/dist/' \
  ./ pi@rover:/home/pi/embedded_projekt/

# dist separat synchronisieren
rsync -avh --delete --exclude '.DS_Store' \
  ./apps/dashboard/dist/ pi@rover:/home/pi/embedded_projekt/apps/dashboard/dist/
```

Auf dem Pi: `sudo systemctl restart embedded-bridge.service`

## Dokumentation

| Dokument | Pfad |
|----------|------|
| Protokoll (SSOT) | `apps/bridge/docs/PROTOCOL.md` |
| Workflow (Build/Deploy/Run) | `apps/bridge/docs/WORKFLOW.md` |
| Smoke-Test (E2E) | `apps/bridge/docs/SMOKE_TEST.md` |
| Dashboard Entwickler-Guide | `apps/dashboard/docs/DEVELOPER.md` |
| Troubleshooting | `apps/dashboard/docs/TROUBLESHOOTING.md` |

## Code-Stil

- **Sprache**: Kommentare und Docstrings sind auf Deutsch
- **Bridge**: Python 3.10+, Type Hints, async/await
- **Dashboard**: TypeScript strict mode, React 18, Tailwind CSS
