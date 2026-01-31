---
title: "SMOKE_TEST"
version: "1.0.0"
updated: "2026-01-31"
scope: "E2E Smoke Test: Browser ↔ WebSocket ↔ Bridge ↔ Serial ↔ ESP32"
---

# Smoke-Test (E2E)

Browser (React SPA) ↔ WebSocket(JSON) ↔ Bridge(Python) ↔ Serial(USB) ↔ ESP32

## Begriffe / Annahmen

- **Bridge**: `apps/bridge/bridge.py` (aiohttp + WebSocket + Serial)
- **UI**: React SPA, Build liegt in `apps/dashboard/dist/`
- **WS Endpoint**: `ws://<host>:<port>/ws`
- **HTTP UI**: `http://<host>:<port>/`
- **One-hot**: `selected=0` = alles aus, `selected=1..N` = genau ein Kanal aktiv

## Ziel und Pass-Kriterium

**Ziel:** Schnell prüfen, ob die komplette Kette funktioniert:
UI → WS → Bridge → Serial → ESP32 → Events zurück an UI.

**Bestanden, wenn:**
- [ ] UI lädt (HTTP 200) und Assets liefern 200.
- [ ] WS verbindet (UI: connected).
- [ ] Bridge sendet nach WS-Connect mindestens `selection` (recover).
- [ ] Mit ESP32: `serial=connected` und `hello` kommt.
- [ ] UI-Click `setSelection` führt zu one-hot Auswahl + `ack` (oder klarer warn/error).

---

## SSOT: Ports und Pfade

### macOS (lokal)
- Projektroot: `~/daten/IoT/embedded_projekt`
- Bridge HTTP/WS: `localhost:8787`

### Pi 5 (prod)
- Projektroot: `/home/pi/embedded_projekt`
- Bridge HTTP/WS: `rover.local:8081` (oder `<pi-ip>:8081`)
- ESP32 steckt am Pi (Serial via `/dev/serial/by-id/...`)

---

## A) macOS – PROD Smoke-Test (UI aus `dist/`, kein Vite)

### A1) React builden
```bash
cd ~/daten/IoT/embedded_projekt/apps/dashboard
npm ci
npm run build
ls -la dist
```

### A2) Bridge starten

```bash
cd ~/daten/IoT/embedded_projekt/apps/bridge

python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install -U pip
python3 -m pip install -r requirements.txt

python3 bridge.py \
  --port auto \
  --vid 0x303A --pid 0x1001 \
  --baud 921600 \
  --http 8787 \
  --web-root ../dashboard/dist
```

### A3) Checks (CLI)

```bash
curl -I http://localhost:8787/
curl -I http://localhost:8787/assets/
```

### A4) Checks (Browser)

* UI: `http://localhost:8787/`
* Settings → WS URL: `ws://localhost:8787/ws`
* Connect → Status muss `connected` zeigen
* Button/LED klicken → `ack` + `selection` Events

Stop:

```bash
deactivate
```

---

## B) macOS – DEV Smoke-Test (Vite + Bridge getrennt)

### B1) Bridge starten (WS + optional static)

```bash
cd ~/daten/IoT/embedded_projekt/apps/bridge
source .venv/bin/activate

python3 bridge.py \
  --port auto \
  --vid 0x303A --pid 0x1001 \
  --baud 921600 \
  --http 8787 \
  --web-root static
```

### B2) Vite starten

```bash
cd ~/daten/IoT/embedded_projekt/apps/dashboard
npm run dev
# UI: http://localhost:5173/
```

Browser:

* UI: `http://localhost:5173/`
* Settings → WS URL: `ws://localhost:8787/ws`
* Connect → connected
* Buttons/LEDs testen → ack/selection

---

## C) Pi 5 – PROD Smoke-Test (Bridge hostet `dist/`) – ESP32 am Pi

### C1) Deploy (Mac → Pi)

Auf macOS:

```bash
cd ~/daten/IoT/embedded_projekt

# Zielpfade sicher anlegen (sonst rsync bricht ab)
ssh pi@rover 'mkdir -p /home/pi/embedded_projekt/apps/dashboard/dist'

# Bridge-Code (ohne .venv)
rsync -avh --delete \
  --exclude '.venv/' \
  --exclude '__pycache__/' --exclude '*.pyc' --exclude '.DS_Store' \
  "./apps/bridge/" \
  "pi@rover:/home/pi/embedded_projekt/apps/bridge/"

# Frontend dist
rsync -avh --delete \
  --exclude '.DS_Store' \
  "./apps/dashboard/dist/" \
  "pi@rover:/home/pi/embedded_projekt/apps/dashboard/dist/"
```

### C2) venv + deps (Pi, einmalig)

Auf Pi:

```bash
ssh rover

sudo apt update
sudo apt install -y python3-venv

mkdir -p /home/pi/venvs
python3 -m venv /home/pi/venvs/selection-bridge
source /home/pi/venvs/selection-bridge/bin/activate

python3 -m pip install -U pip
python3 -m pip install -r /home/pi/embedded_projekt/apps/bridge/requirements.txt

deactivate
```

### C3) Serial-Rechte (Pi, einmalig)

```bash
sudo usermod -aG dialout pi
# danach neu einloggen oder reboot
```

### C4) Manuell starten (Pi)

```bash
cd /home/pi/embedded_projekt/apps/bridge
source /home/pi/venvs/selection-bridge/bin/activate

flock -n /var/lock/esp32-serial.lock \
  python3 bridge.py \
    --port auto \
    --vid 0x303A --pid 0x1001 \
    --baud 921600 \
    --http 8081 \
    --web-root /home/pi/embedded_projekt/apps/dashboard/dist \
    --wait-port --wait-poll 0.5 --wait-timeout 0 \
    --reconnect-delay 2 \
    --ack-timeout 0.6 --retries 4
```

### C5) Checks (Mac im LAN)

```bash
curl -I http://rover.local:8081/
```

Browser:

* UI: `http://rover.local:8081/`
* Settings → WS URL: `ws://rover.local:8081/ws` (nicht `localhost`)
* Connect → connected
* Buttons/LEDs testen → ack/selection

---

## D) systemd (Pi)

### D1) Service steuern

```bash
sudo systemctl daemon-reload
sudo systemctl restart embedded-bridge.service
sudo systemctl status embedded-bridge.service --no-pager -l
sudo journalctl -u embedded-bridge.service -f
```

### D2) Zusätzliche Checks (Pi)

```bash
ss -ltnp | egrep ':8081'
ls -la /dev/serial/by-id/
```

Pass-Kriterium zusätzlich:

* Journal zeigt `http listening ...` und `opening serial ...`
* `/dev/serial/by-id/...Espressif... -> ttyACM*` existiert

---



## Windows (Kurz)

* Start: immer `python bridge.py ...` (nicht `./bridge.py`)
* Port: meist `COMx` explizit setzen

```powershell
py -m venv .venv
.\.venv\Scripts\activate
py -m pip install -U pip
pip install -r requirements.txt

python .\bridge.py --http 8787 --port COM5 --baud 921600 --web-root ..\dashboard\dist
```
