"""
web_app.py – aiohttp Web-App Setup.

Stellt bereit:
- WebSocket-Endpoint /ws für Browser-Kommunikation
- Statische Dateien (Vite-Build aus dist/)
- SPA-Fallback-Routing (alle unbekannten Pfade → index.html)
"""
from __future__ import annotations

from pathlib import Path
from typing import Any

from aiohttp import web, WSMsgType

from bridge_core import Bridge


async def ws_handler(request: web.Request) -> web.WebSocketResponse:
    """
    WebSocket-Handler für Browser-Clients.

    Empfängt Kommandos (setSelection, ping) und leitet sie an die Bridge weiter.
    Sendet nach Verbindung sofort den aktuellen Zustand (recover).
    """
    bridge: Bridge = request.app["bridge"]

    ws = web.WebSocketResponse(heartbeat=20)
    await ws.prepare(request)

    bridge.ws_add(ws)
    await bridge.ws_push_recover_state(ws)

    try:
        async for msg in ws:
            if msg.type == WSMsgType.TEXT:
                try:
                    data = msg.json()
                except Exception:
                    await ws.send_json({"type": "error", "code": 2, "detail": "invalid_json"})
                    continue

                mtype = data.get("type")
                if mtype == "setSelection":
                    # Auswahl setzen (0 = alle aus, 1..N = One-Hot)
                    try:
                        sel = int(data.get("selected", 0))
                    except Exception:
                        sel = 0
                    await bridge.cmd_set_selection(sel)
                elif mtype == "ping":
                    await ws.send_json({"type": "pong"})
                else:
                    await ws.send_json({"type": "error", "code": 3, "detail": "unknown_ws_type"})

            elif msg.type == WSMsgType.ERROR:
                break
    finally:
        bridge.ws_discard(ws)

    return ws


def make_app(bridge: Bridge, web_root: Path) -> web.Application:
    """
    Erstellt die aiohttp-Applikation mit allen Routen.

    Routen:
    - /ws: WebSocket-Endpoint
    - /assets/*: Vite-Build Assets (JS/CSS)
    - /static/*: Legacy/Fallback-Mapping
    - /: index.html
    - /*: SPA-Fallback (alle anderen Pfade → index.html)
    """
    app = web.Application()
    app["bridge"] = bridge

    # WebSocket (muss vor dem Catch-all kommen)
    app.router.add_get("/ws", ws_handler)

    # Vite-Build Assets: dist/assets → /assets/...
    assets_dir = web_root / "assets"
    if assets_dir.is_dir():
        app.router.add_static("/assets/", path=str(assets_dir), show_index=False)

    # Optional: Legacy/Static-Mapping
    app.router.add_static("/static/", path=str(web_root), show_index=False)

    # Root → index.html
    async def index_handler(_request: web.Request) -> web.StreamResponse:
        return web.FileResponse(str(web_root / "index.html"))

    app.router.add_get("/", index_handler)

    # SPA-Fallback (z.B. bei Refresh auf /logs)
    async def spa_fallback(request: web.Request) -> web.StreamResponse:
        rel = request.match_info["path"]
        p = (web_root / rel).resolve()

        # Existierende Datei direkt ausliefern
        if p.is_file() and str(p).startswith(str(web_root)):
            return web.FileResponse(str(p))

        # Sonst: SPA-Route → index.html
        return web.FileResponse(str(web_root / "index.html"))

    app.router.add_get("/{path:.*}", spa_fallback)

    return app
