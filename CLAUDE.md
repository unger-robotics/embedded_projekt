# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Selection-Panel-System: Browser-Dashboard ↔ WebSocket(JSON) ↔ Bridge(Python) ↔ Serial(USB) ↔ ESP32 (One-Hot Selection)

**Components:**
- `firmware/` – ESP32-S3 firmware (PlatformIO, Arduino framework)
- `apps/bridge/` – Python WebSocket ↔ Serial bridge (aiohttp, async)
- `apps/dashboard/` – React SPA (TypeScript, Tailwind CSS)
- `hardware/kicad/` – KiCad 8 schematic/PCB
- `doc/` – Documentation (Markdown SSOT in `doc/md/`, LaTeX in `doc/latex/`)
- `scripts/` – Deployment and utility scripts

## Build Commands

### Firmware (ESP32-S3)
```bash
cd firmware
pio run                              # Build (FreeRTOS, default)
pio run -e seeed_xiao_esp32s3_loop   # Build Loop variant (no FreeRTOS)
pio run -t upload                    # Build and flash
pio run -t upload && pio device monitor  # Flash and monitor
pio device monitor                   # Serial monitor (921600 baud)
```

### Firmware Tests
```bash
cd firmware
pio test -e seeed_xiao_esp32s3_test              # Unit tests (logic only)
pio test -e seeed_xiao_esp32s3_test -f test_selection  # Single test
pio test -e seeed_xiao_esp32s3_e2e               # E2E with hardware
```

### Dashboard (React SPA)
```bash
cd apps/dashboard
npm ci                    # Install dependencies
npm run dev               # Dev server: http://localhost:5173
npm run build             # Production build → dist/
```

### Bridge (Python)
```bash
cd apps/bridge
python -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
python bridge.py --port auto --http 8081 --web-root ../dashboard/dist
```

## Linting & Formatting

### Firmware
```bash
cd firmware
./tools/format.sh    # clang-format
./tools/lint.sh      # cppcheck
```

### Dashboard
```bash
cd apps/dashboard
npm run lint              # ESLint
npm run format            # Prettier
npm run typecheck         # TypeScript
```

### Bridge
```bash
cd apps/bridge
ruff check . && ruff format .   # Linting + formatting
mypy .                          # Type checking
```

## Architecture

### Data Flow
```
Browser (React SPA)
  ↓ WebSocket (JSON)
Bridge (Python: aiohttp)
  ↓ Serial (COBS + CRC16, 921600 Baud)
ESP32 Firmware
  ↓ SPI
Shift Registers (CD4021 buttons, HC595 LEDs)
```

### Firmware Layer Structure
```
src/app/      → Application (io_task/io_loop, serial_bridge)
src/logic/    → Business logic (debounce, selection, io_pipeline) – hardware-independent
src/drivers/  → Chip drivers (cd4021, hc595) – C API
src/hal/      → HAL (spi_bus) – C API with C++ wrapper
src/protocol/ → Protocol (COBS frames, CRC)
include/hw/   → Hardware config SSOT (pins, counts, SPI params, timing)
```

**Entry Point**: `src/main.cpp` dispatches to `app_task` (FreeRTOS) or `app_loop` (polling) based on `APP_USE_FREERTOS`

### Bridge Module Structure
| File | Purpose |
|------|---------|
| `bridge.py` | CLI entry, argument parsing |
| `bridge_core.py` | Serial ↔ WebSocket state machine, reconnect logic |
| `web_app.py` | aiohttp setup, WebSocket handler, SPA routing |
| `protocol.py` | Message types, packet build/parse |
| `cobs.py`, `crc.py` | COBS framing, CRC16 CCITT-FALSE |

### Dashboard Structure
| Directory | Purpose |
|-----------|---------|
| `src/pages/` | Route pages (Dashboard, Logs, Settings) |
| `src/components/` | Reusable UI (ButtonChips, LedGrid, StatusPill) |
| `src/lib/ws/` | WebSocket client, React Context store |

## Protocol

**Serial (ESP32 ↔ Bridge):** COBS-framed, 0x00 delimiter, Header (10B) + Payload + CRC16

| Message | Hex | Direction |
|---------|-----|-----------|
| HELLO | 0x01 | ESP32 → Bridge (on boot) |
| SELECTION_STATE | 0x12 | ESP32 → Bridge (on change) |
| CMD_SET_SELECTION | 0x20 | Bridge → ESP32 |
| ACK | 0x30 | Response when ACK_REQ flag set |

**WebSocket (Browser ↔ Bridge):** Endpoint `/ws`
- Commands (→ Bridge): `setSelection`, `ping`
- Events (← Bridge): `hello`, `selection`, `serial`, `ack`, `warn`, `error`

## Key Constraints

- **One-Hot Selection**: Only one button can be selected at a time (0 = none, 1-10 = selected)
- **Bit Mapping (MSB-First)**: IDs 1-10 map to bits with MSB = ID 1. Formula: `mask = 1 << (7 - ((id-1) % 8))`
- **Port Auto-detection**: USB VID 0x303A, PID 0x1001 (Espressif ESP32-S3)
- **Logic Layer is Hardware-Independent**: `src/logic/` must be testable without hardware

## Coding Standards

### Firmware
- **Basis**: BARR-C (style) + MISRA-oriented rules
- **Language Split**: C for drivers/HAL (with C++ wrapper), C++ for application
- **Naming**: Structs `_t` suffix, enums `_e` suffix, enum values UPPER_SNAKE, classes PascalCase, C functions `prefix_snake`, private members `_` prefix
- **Error Handling**: `error_code_e` from `include/error_codes.h`, return `APP_ERR_SUCCESS` (0) on success
- **C/C++ Interop**: All C headers need `extern "C"` guards

### Apps
- **Language**: Comments and docstrings in German
- **Bridge**: Python 3.10+, type hints, async/await
- **Dashboard**: TypeScript strict mode, React 18, Tailwind CSS

## Git Workflow

**Remote:** `git@github.com:unger-robotics/embedded_projekt.git`

```bash
# Initial setup (already done)
git remote add origin git@github.com:unger-robotics/embedded_projekt.git
git branch -M main
git push -u origin main

# Daily workflow
git pull                              # Fetch and merge
git add <files>                       # Stage changes
git commit -m "feat: description"     # Commit
git push                              # Push to origin

# Commit message prefixes
# feat:     New feature
# fix:      Bug fix
# docs:     Documentation
# refactor: Code refactoring
# test:     Tests
# chore:    Build, config, deps
```

## Deployment (Mac → Pi)

```bash
# Build dashboard
cd apps/dashboard && npm ci && npm run build && cd -

# Sync to Pi (excluding venv/node_modules)
rsync -avh --delete \
  --exclude '.git/' --exclude '.venv/' --exclude '__pycache__/' \
  --exclude '*.pyc' --exclude '.DS_Store' --exclude 'node_modules/' \
  --exclude 'apps/dashboard/dist/' \
  ./ pi@rover:/home/pi/embedded_projekt/

# Sync dist separately
rsync -avh --delete --exclude '.DS_Store' \
  ./apps/dashboard/dist/ pi@rover:/home/pi/embedded_projekt/apps/dashboard/dist/
```

On Pi: `sudo systemctl restart embedded-bridge.service`

## Documentation References

| Topic | Path |
|-------|------|
| Firmware Details | `firmware/CLAUDE.md` |
| Apps Details | `apps/CLAUDE.md` |
| Protocol (SSOT) | `apps/bridge/docs/PROTOCOL.md` |
| Firmware Architecture | `firmware/docs/architecture.md` |
| Firmware API | `firmware/docs/DEVELOPER.md` |
| Algorithms (Debounce, Selection) | `firmware/docs/ALGORITHM.md` |
| Hardware Pinout | `firmware/docs/HARDWARE.md` |
| Coding Standard | `firmware/docs/CODING_STANDARD.md` |
| KiCad Project | `hardware/kicad/selection-panel-prototyp/README.md` |
