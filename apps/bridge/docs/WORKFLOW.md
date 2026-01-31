# Workflow (Build / Deploy / Run)

Ziel: Du baust das Dashboard auf dem Mac, deployst nach Pi, Bridge läuft als systemd-Service (ESP32 am Pi).

## 0) Pfade (Ist-Stand)

**Mac:**
- Projekt: `/Users/jan/daten/IoT/embedded_projekt`
- Bridge: `apps/bridge/`
- Frontend: `apps/dashboard/`

**Pi:**
- Projekt: `/home/pi/embedded_projekt`
- Bridge: `/home/pi/embedded_projekt/apps/bridge`
- Frontend dist: `/home/pi/embedded_projekt/apps/dashboard/dist`

## 1) Build Frontend (Mac)

```bash
cd /Users/jan/daten/IoT/embedded_projekt/apps/dashboard
npm ci
npm run build
# Ergebnis: ./dist
```

## 2) Deploy nach Pi (Mac → Pi)

### A) Bridge-Code (ohne venv)

```bash
cd /Users/jan/daten/IoT/embedded_projekt

rsync -avh --delete \
  --exclude '.venv/' \
  --exclude '__pycache__/' --exclude '*.pyc' --exclude '.DS_Store' \
  "./apps/bridge/" \
  "pi@rover:/home/pi/embedded_projekt/apps/bridge/"
```

### B) Frontend dist (Pfad vorher anlegen)

Auf dem Pi einmalig:

```bash
mkdir -p /home/pi/embedded_projekt/apps/dashboard/dist
```

Dann vom Mac:

```bash
cd /Users/jan/daten/IoT/embedded_projekt

rsync -avh --delete \
  --exclude '.DS_Store' \
  "./apps/dashboard/dist/" \
  "pi@rover:/home/pi/embedded_projekt/apps/dashboard/dist/"
```

## 3) Python venv auf dem Pi (einmalig)

```bash
python3 -m venv /home/pi/venvs/selection-bridge
source /home/pi/venvs/selection-bridge/bin/activate
pip install -U pip
pip install -r /home/pi/embedded_projekt/apps/bridge/requirements.txt
```

## 4) Manuell starten (Pi)

```bash
cd /home/pi/embedded_projekt/apps/bridge

flock -n /var/lock/esp32-serial.lock \
  /home/pi/venvs/selection-bridge/bin/python3 bridge.py \
    --port auto \
    --vid 0x303A --pid 0x1001 \
    --baud 921600 \
    --http 8081 \
    --web-root /home/pi/embedded_projekt/apps/dashboard/dist \
    --wait-port --wait-poll 0.5 --wait-timeout 0 \
    --reconnect-delay 2 \
    --ack-timeout 0.6 --retries 4
```

Browser:

* `http://rover.local:8081/`
* Settings → WS URL: `ws://rover.local:8081/ws`

## 5) systemd Service (Pi)

### 5.1 Unit File

`/etc/systemd/system/embedded-bridge.service`

```ini
[Unit]
Description=Embedded Projekt Bridge (ESP32 Serial -> Web Dashboard)
After=network-online.target
Wants=network-online.target
StartLimitIntervalSec=0

[Service]
Type=simple
User=pi
Group=pi
WorkingDirectory=/home/pi/embedded_projekt/apps/bridge
Environment=PYTHONUNBUFFERED=1

ExecStart=/usr/bin/flock -n /var/lock/esp32-serial.lock \
  /home/pi/venvs/selection-bridge/bin/python3 /home/pi/embedded_projekt/apps/bridge/bridge.py \
    --port auto \
    --vid 0x303A --pid 0x1001 \
    --baud 921600 \
    --http 8081 \
    --web-root /home/pi/embedded_projekt/apps/dashboard/dist \
    --wait-port --wait-timeout 0 --wait-poll 0.5 \
    --reconnect-delay 2 \
    --ack-timeout 0.6 --retries 4

Restart=always
RestartSec=1
TimeoutStopSec=5
KillSignal=SIGTERM

[Install]
WantedBy=multi-user.target
```

### 5.2 Aktivieren / Starten / Logs

```bash
sudo systemctl daemon-reload
sudo systemctl enable embedded-bridge.service
sudo systemctl restart embedded-bridge.service
sudo systemctl status embedded-bridge.service --no-pager -l
sudo journalctl -u embedded-bridge.service -f
```

## 6) Smoke-Test (E2E)

**Pass-Kriterium:**

* WS verbindet (UI: connected)
* Bridge sendet nach Connect mindestens `selection` (recover)
* Serial connected + `hello` kommt
* UI-Click `setSelection` → one-hot + `ack`

**Checks:**

```bash
curl -I http://rover.local:8081/
# im Browser DevTools:
# - WS URL stimmt?
# - /ws handshake ok?
```
