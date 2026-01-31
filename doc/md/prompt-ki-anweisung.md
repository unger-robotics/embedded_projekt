# KI

- **Prüfe** den Inhalt, identifiziere Inkonsistenzen/Fehler und liefere eine vollständig überarbeitete, konsistente Version.

- Abgleich der Doku mit der tatsächlichen Implementierung

- Überarbeite **Prompt** (prägnant, eindeutig):

- **Erkläre** das Thema nach Bloch+Akademisch-Stil

- **Prinzipien:** Arbeite nach *Simple is best*, *Teile und herrsche*, *keine Redundanz* und gängiger Engineering-Praxis;
  - trenne strikt **Inhalt**, **Design** und **Code**.

- **Doku:** erstelle die Dokumentation im Bloch+Akademisch-Stil in **GFM (UTF-8)**

- Prüfe den **Markdown-Text**, formatiere ihn vollständig in GFM (UTF-8) und gib die komplette Ausgabe aus.


**Markdown → LaTeX**
Erzeuge LaTeX-Fragmente ohne Präambel, passend zur main_*.tex und techdoc.cls.
Anforderungen: Unicode-Prüfung, Mathe in LaTeX-Syntax, Einheiten mit \SI{}{},
booktabs-Tabellen, lstlisting (arduino/python/shell), infobox/warnbox/tipbox, \cref{}.

**Für .tex-Prüfung:**
Identifiziere Schriftart-Kompatibilitätsprobleme. Prüfe auf:
- Kritische Unicode: `U+2500-257F (Boxzeichen ─│┌), U+2190-21FF (Pfeile ↑↓)`
- Nicht definierte Symbole: `☐☑★●`
- Einheiten: `Ω→\SI{}{\ohm}, µ→\micro, °→\degree`
Ausgabe: Tabelle mit Zeile, Problem, Korrektur.

- **Vortrag:** Erstelle eine kurze, klare Präsentationsstruktur (Agenda → Kernaussagen → Demo → Architektur → Next Steps) und liefere Folientexte.

- **Git:** Definiere Repo-Standards: Branch/PR-Workflow, Naming, Tags/Releases, CI/Checks, Code-Review-Regeln.

- **Commit-Message:** Erzeuge Commit-Message(s) im **Conventional Commits**-Format passend zu den Änderungen.

- **KiCad 8 Projekt:** Erstelle ein vollständiges KiCad-8-Projekt (Schematic + PCB) nach dem aktuellen Workflow **Schaltplan → PCB** (ohne Netzlisten-Import); liefere die Projektstruktur und alle nötigen Dateien inkl. Symbolen/Footprints-Zuordnung, ERC/DRC-fähig.

---

# Code

**Code:** Refaktoriere ohne Funktionsänderung: entferne Redundanzen, erzwinge konsistente Projektstandards (minimal, wartbar, skalierbar, nicht-blockierend), verbessere Lesbarkeit; Kommentare erklären **warum** (nicht nur **was**).

**Code-Kommentare & README:** Der Code funktioniert; verbessere die Lesbarkeit. Schreibe Kommentare, die **warum** etwas so gelöst ist (nicht nur **was** passiert). Beachte: *Simple is best*, *Teile und herrsche*, *keine Redundanz*, *Engineering-Praxis*. Erstelle zusätzlich eine passende, vollständige `README.md`.

**Firmware-Guide:** Erstelle `docs/firmware-guide.md` als vollständigen, GFM/Pandoc-tauglichen Leitfaden (UTF-8) inkl. Mermaid-Diagrammen; behandle: Embedded vs. Desktop, Arduino C++ (C++11/GCC 8.4 Limits), Embedded-Dogmen (kein Heap/keine Blockierung), Code-Struktur (Includes→`loop()`), Syntax (Typwahl `uint8_t`, `static/const/constexpr`, `inline`, Pointer/Refs/Arrays, Bit-Ops), Arduino-APIs (pinMode/SPI/PWM), Ablauf- & Datenflussdiagramme für den konkreten Code sowie FreeRTOS-Architektur.

**Glossar**: Erkläre alle Fachbegriffe alphabetisch sortiert.

- **CSS:** Analysiere Responsiveness; optimiere Layout für **Fullscreen ohne Scrollen**; definiere ein konsistentes, professionelles Farbschema (Arduino, Raspberry Pi 5) und setze es systematisch um.

- **UI (Interaktives Auswahlpanel):** Implementiere das Auswahlpanel getrennt nach **HTML/CSS/JS** (keine Inline-Styles), modular und erweiterbar.


## React + TypeScript + Vite + eslint/prettier + react-router + Tailwind

### React

**React** ist eine JavaScript-Bibliothek für **Benutzeroberflächen (UI)**. Du baust die Oberfläche aus **Komponenten** (z. B. Button, Statusanzeige, Dashboard-Kachel). React rendert diese Komponenten effizient, wenn sich Daten/State ändern.

**Beispiel:** `ButtonPanel`, das bei Tastendruck den Zustand (`pressed`) aktualisiert und die Ansicht automatisch neu zeichnet.

### TypeScript

**TypeScript (TS)** ist **JavaScript mit Typen**. Du definierst z. B., dass `ledId` eine Zahl ist oder dass ein WebSocket-Event ein bestimmtes Format hat.

**Nutzen:**

* weniger Laufzeitfehler (vieles fällt beim Kompilieren auf)
* bessere Autovervollständigung in VS Code
* saubere Schnittstellen (z. B. für dein JSON-Protokoll ESP32 ↔ Pi ↔ Browser)

### Vite

**Vite** ist ein **Build-Tool/Dev-Server** für Frontend-Projekte:

* startet lokal sehr schnell
* liefert “Hot Reload” (Änderung speichern → Browser aktualisiert sofort)
* baut am Ende eine optimierte Produktionsversion (minified assets)

### UI/Styling

* **CSS direkt** geht immer.
* **Tailwind CSS** (schnell konsistente UI bauen).

### Routing

* **react-router** Wenn du mehrere Seiten willst (z. B. “Dashboard”, “Settings”)

### Daten vom Pi/ESP32

Wenn du mit deinem Pi-Service redest:

* **WebSocket** (live Status/Events)
* oder **HTTP/REST** (Konfiguration, Listen, Dateien)
  Das geht mit Bordmitteln (`fetch`, `WebSocket`) – kein Extra zwingend nötig.

### Qualität / Wartbarkeit

* **ESLint** + **Prettier** (Format/Regeln)
* **Vitest** (Tests)

---

### 1) Lokale Web-Apps entwickeln

* Dashboards (z. B. für dein Selection-Panel)
* Visualisierung von Button/LED-States
* Audio/Bild/Video-UI im Browser
* Konfigurationsseiten (Mapping, Profile, Debug-Ansicht)

### 2) Als lokale Oberfläche für deinen Raspberry Pi-Service

Typisches Setup:

* Pi läuft als Server (Python aiohttp / Node) und bietet WebSocket/HTTP
* Dein Mac (oder Pi selbst) zeigt die React-Webseite im Browser
* Ideal für: Live-Status, Logs, “One-Hot”-Validierung, Reconnect-Handling

### 3) Als “statische” Website bauen und irgendwo hosten

Vite erzeugt am Ende einen `dist/`-Ordner. Den kannst du:

* auf dem Pi via nginx/Static-Server ausliefern

---

erstelle Single-Page-App (SPA) Dashboard im Browser ziel Embedded-Entwicklung (Farbdesign: Arduino, Raspi 5)

starte mit **(A) Dashboard im Browser** html, css, js

weiter, frage mich vorher **(B) Dashboard im Browser** saubere Projektstruktur (React + TS + Vite, eslint/prettier, react-router, Tailwind) inkl. WebSocket-Event-Typen in TypeScript.


Zielgruppe: Entwickler und Benutzer und Admins
erstelle je README.md/ DEVELOPER.md / PROTOCOL.md / WORKFLOW.md



---

# README

## Lizenz

MIT License

## Autor & Maintainer

Jan Unger

## Credits

Unterstützt durch KI-Tools (Claude Opus 4.5, ChatGPT 5.2).

# Präsentation

**letzte Folie** „Credits“

Autor: Jan Unger

Unterstützung: Claude Opus 4.5, ChatGPT 5.2 (Text-/Code-Vorschläge)

Stand: 2026-01-08

# Dokumentation

## Autor

Jan Unger

## Tools / Credits

Claude Opus 4.5, ChatGPT 5.2 (Unterstützung bei Formulierung und Code-Vorschlägen)

Stand: 2026-01-08
