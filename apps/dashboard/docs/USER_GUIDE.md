# Dashboard UI – Benutzeranleitung

## Übersicht

Das Dashboard steuert eine **One-Hot Selection**:
- `0` = alles aus
- `1..N` = genau eine Auswahl aktiv

![Dashboard Screenshot](Embedded%20Dashboard_pi5.png)

## 1) Verbinden

1. Öffne die UI im Browser (z.B. `http://rover.local:8081/`)
2. Prüfe den Status oben rechts:
   - **CONNECTED** (grün) = WebSocket verbunden
   - **DISCONNECTED** (rot) = keine Verbindung

## 2) Auswahl setzen

- Klicke einen Button (`BTN 1` bis `BTN N`)
- Ergebnis:
  - Status zeigt `Selection = <id>`
  - Genau eine LED leuchtet (One-Hot)
  - ESP32 schaltet entsprechend

## 3) Auswahl löschen

- Klicke `Clear (0)` um alle LEDs auszuschalten
- Setzt `selected = 0`

## 4) Seiten

| Seite | Funktion |
|-------|----------|
| **Dashboard** | Buttons + LEDs + aktueller Status |
| **Logs** | Event-Verlauf (selection, ack, error) |
| **Settings** | WS-URL konfigurieren |

## 5) WS-URL einstellen

Wenn die UI nicht lokal läuft, muss die WS-URL angepasst werden:

| Situation | WS-URL |
|-----------|--------|
| UI auf Pi (`rover.local:8081`) | `ws://rover.local:8081/ws` |
| UI lokal (Dev, `localhost:5173`) | `ws://localhost:8787/ws` |

**Settings → WebSocket URL → Speichern**
