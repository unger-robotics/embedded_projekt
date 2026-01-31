#!/usr/bin/env python3
"""
bridge.py – CLI-Einstiegspunkt für die Bridge.

Startet HTTP/WebSocket-Server und Serial-Verbindung zum ESP32.
Konfiguration erfolgt über Kommandozeilenargumente.
"""
from __future__ import annotations

import argparse
import asyncio
from pathlib import Path

from aiohttp import web

from bridge_core import Bridge
from util import log
from web_app import make_app


async def main() -> None:
    ap = argparse.ArgumentParser(description="ESP32 Serial ↔ WebSocket Bridge")

    # Serial-Konfiguration
    ap.add_argument("--port", default="auto", help="Gerätepfad oder 'auto' für Autodetect")
    ap.add_argument("--baud", type=int, default=921600, help="Baudrate (Standard: 921600)")
    ap.add_argument("--http", type=int, default=8081, help="HTTP/WS-Port (Standard: 8081)")

    # USB-Autodetect-Filter
    ap.add_argument(
        "--by-id-substr",
        default="Espressif_USB_JTAG_serial_debug_unit",
        help="Substring-Match in /dev/serial/by-id (Linux)",
    )
    ap.add_argument("--vid", type=lambda x: int(x, 0), default=None, help="USB Vendor ID (z.B. 0x303A)")
    ap.add_argument("--pid", type=lambda x: int(x, 0), default=None, help="USB Product ID (z.B. 0x1001)")

    # ACK-Protokoll
    ap.add_argument("--ack-timeout", type=float, default=0.2, help="ACK-Timeout in Sekunden")
    ap.add_argument("--retries", type=int, default=3, help="Anzahl Wiederholungsversuche bei ACK-Timeout")

    # Warte-/Reconnect-Verhalten (robust für systemd)
    ap.add_argument("--wait-port", action="store_true", help="Warte bis Serial-Gerät existiert")
    ap.add_argument("--wait-poll", type=float, default=0.5, help="Poll-Intervall in Sekunden")
    ap.add_argument("--wait-timeout", type=float, default=0.0, help="Timeout in Sekunden (0=unbegrenzt)")
    ap.add_argument("--reconnect-delay", type=float, default=2.0, help="Verzögerung vor Reconnect in Sekunden")

    # Web-Root (Vite-Build)
    ap.add_argument("--web-root", default="static", help="Verzeichnis mit index.html (Standard: ./static)")

    args = ap.parse_args()

    # Web-Root validieren
    base_dir = Path(__file__).resolve().parent
    web_root = (base_dir / args.web_root).resolve()
    if not (web_root / "index.html").exists():
        raise SystemExit(f"web-root ungültig (index.html fehlt): {web_root}")

    # Bridge und Web-App initialisieren
    bridge = Bridge(ack_timeout_s=args.ack_timeout, retries=args.retries)

    app = make_app(bridge, web_root)
    runner = web.AppRunner(app)
    await runner.setup()

    # HTTP-Server auf IPv4 + IPv6 starten
    site4 = web.TCPSite(runner, "0.0.0.0", args.http)
    site6 = web.TCPSite(runner, "::", args.http)
    await site4.start()
    await site6.start()

    assets_note = "assets=on" if (web_root / "assets").is_dir() else "assets=off"
    log(f"HTTP lauscht auf 0.0.0.0:{args.http} und [::]:{args.http}  web_root={web_root}  {assets_note}")

    try:
        # Serial-Verbindung mit automatischem Reconnect
        await bridge.serial_connect_loop(
            port_arg=args.port,
            by_id_substr=args.by_id_substr,
            vid=args.vid,
            pid=args.pid,
            baud=args.baud,
            wait_port_enabled=args.wait_port,
            wait_poll_s=args.wait_poll,
            wait_timeout_s=args.wait_timeout,
            reconnect_delay_s=args.reconnect_delay,
        )
    finally:
        await bridge.close_serial()
        await runner.cleanup()


if __name__ == "__main__":
    asyncio.run(main())
