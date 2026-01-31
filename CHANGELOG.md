# Changelog

Alle relevanten Aenderungen am Projekt werden in dieser Datei dokumentiert.

Format basiert auf [Keep a Changelog](https://keepachangelog.com/de/1.0.0/).
Versionierung folgt [Semantic Versioning](https://semver.org/lang/de/).

## [Unreleased]

### Hinzugefuegt
- `CLAUDE.md` auf Root-Ebene fuer Claude Code Guidance
- Verbesserte Projektstruktur-Dokumentation in `README.md`

### Geaendert
- `README.md` gestrafft, operative Details nach `CLAUDE.md` verschoben
- `.gitignore` erweitert (KiCad-Backups, dist/, .env, Logs)

### Behoben
- `apps/dashboard/.gitignore` repariert (war beschaedigt)

---

## [1.0.0] - 2025-01-31

Erster stabiler Release des Selection-Panel-Systems.

### Hinzugefuegt

**Firmware (ESP32-S3)**
- One-Hot Selection mit 10 Kanaelen (Buttons + LEDs)
- Dual-Core FreeRTOS-Variante (io_task + serial_task)
- Single-Core Loop-Variante (ohne FreeRTOS)
- Debounce-Algorithmus fuer Tastereingaben
- Serial-Protokoll mit COBS-Framing und CRC16
- Hardware-Treiber fuer CD4021 (Buttons) und HC595 (LEDs)
- Unit-Tests fuer Logic-Schicht

**Bridge (Python)**
- WebSocket ↔ Serial Bridge mit aiohttp
- Automatische Port-Erkennung (USB VID/PID)
- Reconnect-Logik bei Verbindungsabbruch
- SPA-Fallback-Routing fuer Dashboard
- ACK/Retry-Mechanismus fuer Kommandos

**Dashboard (React SPA)**
- Echtzeit-Statusanzeige via WebSocket
- Kanalauswahl per Klick
- Log-Ansicht fuer Debugging
- Responsive UI mit Tailwind CSS

**Hardware (KiCad)**
- Schaltplan mit ESP32-S3-XIAO, 2x HC595, 2x CD4021
- PCB-Layout (Prototyp)

**Dokumentation**
- Protokoll-Spezifikation (SSOT)
- Architektur- und Entwickler-Dokumentation
- Coding Standard (BARR-C / MISRA)
- Deployment-Workflow (Mac → Pi)
