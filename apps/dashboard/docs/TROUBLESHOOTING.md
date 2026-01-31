# Dashboard UI – Troubleshooting

## Problem: UI zeigt "DISCONNECTED"

**Symptom:** Status-Pill ist rot, keine Verbindung zur Bridge.

**Ursachen & Lösungen:**

1. **WS-URL falsch**
   - Browser-Konsole zeigt: `WebSocket connection to 'ws://localhost:...' failed`
   - Lösung: Settings → WS-URL auf den Host der UI setzen
   - Beispiel: UI auf `rover.local:8081` → WS-URL: `ws://rover.local:8081/ws`

2. **Bridge läuft nicht**
   - Prüfen: `curl -I http://localhost:8787/` oder `http://rover.local:8081/`
   - Lösung: Bridge starten (siehe `apps/bridge/docs/README.md`)

3. **Firewall blockiert**
   - Port 8081 (Pi) oder 8787 (lokal) muss erreichbar sein
   - macOS: Systemeinstellungen → Firewall prüfen

## Problem: Buttons reagieren nicht

**Symptom:** Klick auf Button, aber keine Änderung.

**Ursachen & Lösungen:**

1. **Serial nicht verbunden**
   - Logs-Seite zeigt: `serial: disconnected`
   - Lösung: ESP32 anschließen, Bridge neu starten

2. **ACK-Timeout**
   - Logs-Seite zeigt: `warn: ack_timeout`
   - Lösung: ESP32-Verbindung prüfen, ggf. USB-Kabel wechseln

## Problem: Falsche Button-Anzahl

**Symptom:** UI zeigt mehr/weniger Buttons als erwartet.

**Ursache:** Button-Anzahl kommt vom ESP32 via `hello`-Nachricht.

**Lösung:**
1. Logs-Seite öffnen
2. Nach `hello` Event suchen → zeigt `btnCount`
3. Falls falsch: Firmware-Konfiguration prüfen

## Problem: UI lädt nicht (weißer Bildschirm)

**Ursachen & Lösungen:**

1. **Build fehlt**
   - Lösung: `npm run build` ausführen
   - Bridge mit `--web-root ../dashboard/dist` starten

2. **Assets-Pfad falsch**
   - Browser-Konsole zeigt 404 für `/assets/...`
   - Lösung: Bridge-Log prüfen (`assets=on` muss erscheinen)

## Debug-Tipps

1. **Browser DevTools öffnen** (F12)
   - Console: JavaScript-Fehler
   - Network: WebSocket-Verbindung prüfen

2. **Logs-Seite nutzen**
   - Zeigt alle WS-Events
   - Hilfreich für ACK-Timeouts und Protokoll-Fehler

3. **Bridge-Logs prüfen**
   ```bash
   # Pi
   sudo journalctl -u embedded-bridge.service -f

   # Lokal
   # Bridge-Ausgabe im Terminal beobachten
   ```
