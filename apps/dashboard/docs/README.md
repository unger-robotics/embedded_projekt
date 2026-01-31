# Dashboard UI – Dokumentation

Diese Doku beschreibt die Browser-UI (React SPA) für das Selection-Panel.

## Schnellstart

```bash
cd apps/dashboard
npm ci
npm run dev
# UI: http://localhost:5173/
```

Für Produktion:
```bash
npm run build
# dist/ wird von der Bridge ausgeliefert
```

## Dokumentation

| Dokument | Inhalt |
|----------|--------|
| `USER_GUIDE.md` | Benutzeranleitung (UI bedienen) |
| `DEVELOPER.md` | Entwickler-Guide (Build, Lint, Struktur) |
| `TROUBLESHOOTING.md` | Fehlerbehebung |

## Verwandte Dokumentation (Bridge)

| Dokument | Inhalt |
|----------|--------|
| `apps/bridge/docs/PROTOCOL.md` | WebSocket + Serial Protokoll (SSOT) |
| `apps/bridge/docs/WORKFLOW.md` | Build/Deploy/Run Workflow |
| `apps/bridge/docs/SMOKE_TEST.md` | E2E Smoke-Tests |
