# Bridge Docs

Diese Dokumente sind die SSOT für Bridge, Protokoll, Workflow und Smoke-Tests.

- Protokoll (WS + Serial): `PROTOCOL.md`
- Workflow (Build/Deploy/Run): `WORKFLOW.md`
- Smoke-Test (E2E): `SMOKE_TEST.md`

## Schnellstart

**1) Setup (einmalig):**
```bash
cd apps/bridge
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

**2) Dashboard bauen (einmalig):**
```bash
cd apps/dashboard
npm ci && npm run build
```

**3) Bridge starten:**
```bash
cd apps/bridge
source .venv/bin/activate
python bridge.py --port auto --http 8787 --web-root ../dashboard/dist
```

**4) Browser öffnen:**
- UI: `http://localhost:8787/`
- Settings → WS URL: `ws://localhost:8787/ws`

## Wichtige Optionen

| Option | Beschreibung |
|--------|--------------|
| `--port auto` | ESP32 automatisch erkennen |
| `--port /dev/ttyACM0` | Expliziter Serial-Port |
| `--http 8787` | HTTP/WebSocket-Port |
| `--web-root <pfad>` | Pfad zum Dashboard-Build (dist/) |
| `--wait-port` | Warten bis ESP32 angesteckt wird |
| `--vid 0x303A --pid 0x1001` | USB-Filter (Espressif ESP32-S3) |
| `--ack-timeout 0.6` | ACK-Timeout in Sekunden |
| `--retries 4` | Wiederholungsversuche bei Timeout |
