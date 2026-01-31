# Dashboard UI – Entwickler-Guide

## Schnellstart

```bash
cd apps/dashboard
npm ci
npm run dev
# UI: http://localhost:5173/
```

## Befehle

| Befehl | Beschreibung |
|--------|--------------|
| `npm run dev` | Vite Dev-Server (Hot Reload) |
| `npm run build` | Production Build → `dist/` |
| `npm run lint` | ESLint prüfen |
| `npm run format` | Prettier formatieren |
| `npm run format:check` | Prettier prüfen (ohne Änderung) |
| `npm run typecheck` | TypeScript Typ-Prüfung |

## Projektstruktur

```
src/
├── main.tsx          # Einstiegspunkt
├── App.tsx           # Router + Layout
├── index.css         # Tailwind CSS
├── pages/            # Routen-Seiten
│   ├── DashboardPage.tsx   # Hauptseite (Selection)
│   ├── LogsPage.tsx        # Event-Log
│   └── SettingsPage.tsx    # WS-URL Einstellungen
├── components/       # Wiederverwendbare UI
│   ├── Topbar.tsx          # Navigation
│   ├── Card.tsx            # Container
│   ├── StatusPill.tsx      # Status-Anzeige
│   ├── ButtonChips.tsx     # Button-Auswahl
│   └── LedGrid.tsx         # LED-Status
└── lib/ws/           # WebSocket-Client
    ├── client.ts           # WsClient Klasse
    ├── store.tsx           # React Context + State
    ├── codec.ts            # Nachricht-Encoding
    └── types.ts            # TypeScript Types
```

## WS-URL Regel

**UI-Host und WS-Host müssen zusammenpassen:**

| UI läuft auf | WS-URL muss sein |
|--------------|------------------|
| `localhost:5173` (Dev) | `ws://localhost:8787/ws` |
| `rover.local:8081` (Pi) | `ws://rover.local:8081/ws` |

Die WS-URL wird in Settings gespeichert (localStorage).

## Tech Stack

- **React 18** + React Router 7
- **TypeScript** (strict mode)
- **Vite 7** (Build + Dev-Server)
- **Tailwind CSS 4**
- **ESLint** + **Prettier**

## Produktion

Die UI ist statisch (Vite Build). Die Bridge liefert `dist/` aus.

```bash
npm run build
# Dann Bridge mit --web-root ../dashboard/dist starten
```

Siehe: `apps/bridge/docs/WORKFLOW.md`
