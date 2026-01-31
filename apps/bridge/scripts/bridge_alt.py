#!/usr/bin/env python3
"""
bridge_alt.py – Alternative Bridge-Implementierung (alles in einer Datei).

Diese Datei ist eine standalone Version der Bridge, die keine separaten
Module benötigt. Nützlich für schnelle Tests oder als Referenz.

Für die Produktion die modulare Version (bridge.py + bridge_core.py etc.) verwenden.
"""
import asyncio
import struct
import argparse
import glob
import os
import time
from pathlib import Path
from aiohttp import web, WSMsgType

import serial_asyncio
from serial.tools import list_ports


# ---- Protokoll-Konstanten ----------------------------------------------------

MAGIC0 = ord("S")  # 0x53
MAGIC1 = ord("P")  # 0x50
VER_MAJOR = 1

# Message-Types
TYPE_HELLO = 0x01              # ESP32 → Bridge: Konfiguration nach Boot
TYPE_SELECTION_STATE = 0x12    # ESP32 → Bridge: Auswahlzustand geändert
TYPE_CMD_SET_SELECTION = 0x20  # Bridge → ESP32: Auswahl setzen
TYPE_ACK = 0x30                # ESP32 → Bridge: Bestätigung

# Flags
FLAG_ACK_REQ = 0x01  # Sender erwartet ACK
FLAG_IS_ACK = 0x02   # Reserviert

# Wire-Framing
FRAME_DELIM = b"\x00"  # COBS-Delimiter
MAX_FRAME = 2048       # Maximale Frame-Größe (ohne Delimiter)
MAX_RX_BUF = MAX_FRAME + 1

# Source-Namen für SELECTION_STATE
SRC_NAMES = ["local_btn", "host_cmd", "reset", "recover"]


# ---- Hilfsfunktionen ---------------------------------------------------------

def log(msg: str) -> None:
    """Gibt eine Log-Nachricht mit Zeitstempel aus."""
    ts = time.strftime("%Y-%m-%d %H:%M:%S")
    print(f"{ts} [bridge] {msg}", flush=True)


def crc16_ccitt_false(data: bytes) -> int:
    """Berechnet CRC-16/CCITT-FALSE."""
    crc = 0xFFFF
    for b in data:
        crc ^= (b << 8) & 0xFFFF
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def cobs_encode(data: bytes) -> bytes:
    """Codiert Daten mit COBS (Consistent Overhead Byte Stuffing)."""
    out = bytearray()
    idx = 0
    while idx < len(data):
        code_pos = len(out)
        out.append(0)  # Platzhalter für Code
        code = 1
        while idx < len(data) and data[idx] != 0 and code < 0xFF:
            out.append(data[idx])
            idx += 1
            code += 1
        out[code_pos] = code
        if idx < len(data) and data[idx] == 0:
            idx += 1
    return bytes(out)


def cobs_decode(data: bytes) -> bytes:
    """Decodiert COBS-codierte Daten."""
    out = bytearray()
    idx = 0
    while idx < len(data):
        code = data[idx]
        if code == 0:
            raise ValueError("COBS Code=0 ungültig")
        idx += 1
        for _ in range(code - 1):
            if idx >= len(data):
                raise ValueError("COBS Overrun")
            out.append(data[idx])
            idx += 1
        if code != 0xFF and idx < len(data):
            out.append(0)
    return bytes(out)


def build_packet(msg_type: int, flags: int, seq: int, payload: bytes) -> bytes:
    """Baut ein Paket aus Header + Payload + CRC."""
    hdr = struct.pack(
        "<BBBBBBHH",
        MAGIC0, MAGIC1, VER_MAJOR, 0,
        msg_type & 0xFF, flags & 0xFF,
        seq & 0xFFFF, len(payload) & 0xFFFF,
    )
    crc = crc16_ccitt_false(hdr + payload)
    return hdr + payload + struct.pack("<H", crc)


def parse_packet(raw: bytes):
    """Parst ein empfangenes Paket und validiert CRC."""
    if len(raw) < 12:
        raise ValueError("zu kurz")
    magic0, magic1, vmaj, vmin, mtype, flags, seq, ln = struct.unpack("<BBBBBBHH", raw[:10])
    if magic0 != MAGIC0 or magic1 != MAGIC1:
        raise ValueError("falsches Magic")
    if vmaj != VER_MAJOR:
        raise ValueError("falsche Major-Version")
    need = 10 + ln + 2
    if len(raw) != need:
        raise ValueError("falsche Länge")
    payload = raw[10:10 + ln]
    (crc_rx,) = struct.unpack("<H", raw[10 + ln:10 + ln + 2])
    crc_ok = crc16_ccitt_false(raw[:10 + ln])
    if crc_rx != crc_ok:
        raise ValueError("CRC-Fehler")
    return mtype, flags, seq, payload


# ---- Serial-Port-Erkennung ---------------------------------------------------

def autodetect_port(by_id_substr: str | None, vid: int | None, pid: int | None) -> str | None:
    """Erkennt automatisch den Serial-Port des ESP32."""
    # 1) Linux: stabiler Pfad über /dev/serial/by-id
    if os.path.isdir("/dev/serial/by-id"):
        cands = sorted(glob.glob("/dev/serial/by-id/*"))
        if by_id_substr:
            cands = [c for c in cands if by_id_substr in os.path.basename(c)]
        if cands:
            return cands[0]

    # 2) Plattformübergreifend: list_ports (VID/PID optional)
    ports = list(list_ports.comports())
    if vid is not None and pid is not None:
        ports = [p for p in ports if p.vid == vid and p.pid == pid]

    ports.sort(key=lambda p: (("ttyACM" not in p.device and "ttyUSB" not in p.device), p.device))
    if ports:
        return ports[0].device

    return None


async def wait_for_port(
    port_arg: str,
    by_id_substr: str | None,
    vid: int | None,
    pid: int | None,
    poll_s: float,
    timeout_s: float,
) -> str:
    """
    Wartet bis ein nutzbarer Serial-Port verfügbar ist.

    - port_arg="auto": wartet bis Autodetect einen Port findet
    - sonst: wartet bis der Pfad existiert
    """
    t0 = time.monotonic()
    while True:
        if port_arg == "auto":
            p = autodetect_port(by_id_substr, vid, pid)
            if p:
                return p
        else:
            if os.path.exists(port_arg):
                return port_arg

        if timeout_s > 0 and (time.monotonic() - t0) > timeout_s:
            raise TimeoutError("port_wait_timeout")

        await asyncio.sleep(poll_s)


# ---- Bridge-Klasse -----------------------------------------------------------

class Bridge:
    """
    Zentrale Bridge-Klasse.

    Verantwortlichkeiten:
    - Serial-Verbindung mit Reconnect-Logik
    - Protokoll-Handling (HELLO/SELECTION/ACK)
    - WebSocket-Broadcast an verbundene Clients
    """

    def __init__(self, ack_timeout_s: float, retries: int):
        self.ws_clients: set[web.WebSocketResponse] = set()

        self.reader: asyncio.StreamReader | None = None
        self.writer: asyncio.StreamWriter | None = None

        self.tx_seq = 1
        self.tx_lock = asyncio.Lock()

        # Aus HELLO gelernt
        self.btn_count = 10
        self.led_count = 10

        self.ack_timeout_s = ack_timeout_s
        self.retries = retries
        self.pending_acks: dict[int, asyncio.Future] = {}

        # Letzter bekannter Zustand für "recover" nach WS-Reconnect
        self.last_selected = 0
        self.last_hello: dict | None = None
        self.last_port: str | None = None

    def serial_connected(self) -> bool:
        """Prüft ob Serial-Verbindung aktiv ist."""
        return self.writer is not None

    async def open_serial(self, port: str, baud: int):
        """Öffnet die Serial-Verbindung."""
        self.reader, self.writer = await serial_asyncio.open_serial_connection(
            url=port,
            baudrate=baud,
            limit=MAX_RX_BUF,  # Wichtig für readuntil()
        )
        self.last_port = port

    async def close_serial(self):
        """Schließt die Serial-Verbindung und räumt auf."""
        # Ausstehende ACKs fehlschlagen lassen
        for _, fut in list(self.pending_acks.items()):
            if not fut.done():
                fut.set_exception(RuntimeError("serial_disconnected"))
        self.pending_acks.clear()

        if self.writer is not None:
            self.writer.close()
            try:
                await self.writer.wait_closed()
            except Exception:
                pass
        self.reader = None
        self.writer = None

    async def serial_reader_once(self):
        """Liest und verarbeitet ein einzelnes Frame."""
        assert self.reader is not None

        while True:
            frame = await self.reader.readuntil(FRAME_DELIM)  # Wirft bei Disconnect/Overrun
            if not frame:
                continue

            if len(frame) > MAX_FRAME:
                await self.broadcast({"type": "error", "code": 12, "detail": "frame_over_max"})
                continue

            frame = frame[:-1]  # Delimiter entfernen
            if not frame:
                continue

            raw = cobs_decode(frame)
            mtype, flags, seq, payload = parse_packet(raw)
            await self.handle_serial(mtype, flags, seq, payload)

    async def serial_reader_task(self):
        """Reader-Task: läuft bis Disconnect oder Abbruch."""
        try:
            while True:
                try:
                    await self.serial_reader_once()
                except asyncio.LimitOverrunError as e:
                    # Konsumierte Bytes verwerfen und resynchronisieren
                    await self.reader.readexactly(e.consumed)
                    await self.broadcast({"type": "warn", "code": 11, "detail": "frame_too_long"})
                except asyncio.IncompleteReadError:
                    raise RuntimeError("serial_closed")
        except asyncio.CancelledError:
            raise

    async def handle_serial(self, mtype: int, flags: int, seq: int, payload: bytes):
        """Verarbeitet empfangene Serial-Nachrichten nach Typ."""

        # HELLO: ESP32 meldet sich mit Konfiguration
        if mtype == TYPE_HELLO and len(payload) >= 10:
            btn_count, led_count, scan_ms, debounce_ms = struct.unpack("<HHHH", payload[:8])
            feature_bits = payload[8]

            self.btn_count = int(btn_count) if btn_count > 0 else 1
            self.led_count = int(led_count) if led_count > 0 else 1

            msg = {
                "type": "hello",
                "btnCount": self.btn_count,
                "ledCount": self.led_count,
                "scanPeriodMs": int(scan_ms),
                "debounceMs": int(debounce_ms),
                "features": int(feature_bits),
            }
            self.last_hello = msg
            await self.broadcast(msg)
            return

        # SELECTION_STATE: Auswahlzustand hat sich geändert
        if mtype == TYPE_SELECTION_STATE and len(payload) == 8:
            (selected_id,) = struct.unpack("<H", payload[:2])
            source = payload[2]
            (t_ms,) = struct.unpack("<I", payload[4:8])

            self.last_selected = int(selected_id)

            await self.broadcast({
                "type": "selection",
                "selected": int(selected_id),
                "source": SRC_NAMES[source] if source < len(SRC_NAMES) else "unknown",
                "tMs": int(t_ms),
            })
            return

        # ACK: Bestätigung für gesendetes Kommando
        if mtype == TYPE_ACK:
            fut = self.pending_acks.pop(seq, None)
            if fut is not None and not fut.done():
                fut.set_result(True)
            await self.broadcast({"type": "ack", "seq": int(seq)})
            return

    async def serial_send(self, raw: bytes):
        """Sendet Rohdaten COBS-codiert über Serial."""
        if self.writer is None:
            raise RuntimeError("serial_not_connected")
        wire = cobs_encode(raw) + FRAME_DELIM
        self.writer.write(wire)
        await self.writer.drain()

    async def cmd_set_selection(self, selected: int):
        """
        Host-Kommando: setSelection (0..btn_count).

        Verwendet ACK mit Retry. selected=0 bedeutet "alle aus".
        """
        if not self.serial_connected():
            await self.broadcast({"type": "error", "code": 30, "detail": "serial_not_connected"})
            return

        # Wert auf 0..btn_count begrenzen (0=alle aus)
        if selected < 0:
            selected = 0
        if selected > self.btn_count:
            selected = self.btn_count

        async with self.tx_lock:
            seq = self.tx_seq
            self.tx_seq = (self.tx_seq + 1) & 0xFFFF
            if self.tx_seq == 0:
                self.tx_seq = 1

            # Payload: <HBB> (selected, reserved0, reserved1)
            payload = struct.pack("<HBB", selected & 0xFFFF, 0, 0)
            raw = build_packet(TYPE_CMD_SET_SELECTION, FLAG_ACK_REQ, seq, payload)

            last_err = None
            for attempt in range(self.retries + 1):
                fut = asyncio.get_running_loop().create_future()
                self.pending_acks[seq] = fut

                try:
                    await self.serial_send(raw)
                    await asyncio.wait_for(fut, timeout=self.ack_timeout_s)
                    return  # Erfolg
                except asyncio.TimeoutError as e:
                    last_err = e
                    self.pending_acks.pop(seq, None)
                    await self.broadcast({
                        "type": "warn",
                        "code": 20,
                        "detail": f"ack_timeout seq={seq} attempt={attempt+1}/{self.retries+1}",
                    })
                except Exception as e:
                    self.pending_acks.pop(seq, None)
                    await self.broadcast({"type": "error", "code": 22, "detail": f"serial_send_failed: {e}"})
                    return

            await self.broadcast({"type": "error", "code": 21, "detail": f"ack_failed seq={seq}"})
            if last_err:
                raise last_err

    async def broadcast(self, obj: dict):
        """Sendet ein JSON-Objekt an alle verbundenen WS-Clients."""
        dead = []
        for ws in self.ws_clients:
            try:
                await ws.send_json(obj)
            except Exception:
                dead.append(ws)
        for ws in dead:
            self.ws_clients.discard(ws)

    async def serial_connect_loop(
        self,
        port_arg: str,
        by_id_substr: str | None,
        vid: int | None,
        pid: int | None,
        baud: int,
        wait_port_enabled: bool,
        wait_poll_s: float,
        wait_timeout_s: float,
        reconnect_delay_s: float,
    ):
        """Reconnect-Loop: verbindet sich endlos bis zum Abbruch."""
        while True:
            try:
                port = port_arg
                if wait_port_enabled:
                    port = await wait_for_port(port_arg, by_id_substr, vid, pid, wait_poll_s, wait_timeout_s)
                elif port_arg == "auto":
                    port = autodetect_port(by_id_substr, vid, pid) or "/dev/ttyACM0"

                log(f"Öffne Serial: {port} @ {baud}")
                await self.open_serial(port, baud)
                await self.broadcast({"type": "serial", "state": "connected", "port": port})

                # Reader läuft bis Disconnect
                await self.serial_reader_task()

            except asyncio.CancelledError:
                break
            except Exception as e:
                await self.broadcast({"type": "warn", "code": 40, "detail": f"serial_reconnect: {e}"})
                log(f"Serial-Fehler: {e} (Reconnect in {reconnect_delay_s}s)")
            finally:
                try:
                    await self.close_serial()
                except Exception:
                    pass
                await self.broadcast({"type": "serial", "state": "disconnected"})

            await asyncio.sleep(reconnect_delay_s)


# ---- Web-App -----------------------------------------------------------------

async def ws_handler(request):
    """WebSocket-Handler für Browser-Clients."""
    bridge: Bridge = request.app["bridge"]
    ws = web.WebSocketResponse(heartbeat=20)
    await ws.prepare(request)
    bridge.ws_clients.add(ws)

    # Letzten bekannten Zustand an neuen Client senden (recover)
    if bridge.last_hello:
        await ws.send_json(bridge.last_hello)
    if bridge.last_selected is not None:
        await ws.send_json({"type": "selection", "selected": int(bridge.last_selected), "source": "recover", "tMs": 0})
    if bridge.last_port:
        await ws.send_json({
            "type": "serial",
            "state": "connected" if bridge.serial_connected() else "disconnected",
            "port": bridge.last_port,
        })

    try:
        async for msg in ws:
            if msg.type == WSMsgType.TEXT:
                try:
                    data = msg.json()
                except Exception:
                    await ws.send_json({"type": "error", "code": 2, "detail": "invalid_json"})
                    continue

                if data.get("type") == "setSelection":
                    # Auswahl setzen (0 = alle aus, 1..N = One-Hot)
                    try:
                        sel = int(data.get("selected", 0))
                    except Exception:
                        sel = 0
                    await bridge.cmd_set_selection(sel)

            elif msg.type == WSMsgType.ERROR:
                break
    finally:
        bridge.ws_clients.discard(ws)

    return ws


def make_app(bridge: Bridge, web_root: Path) -> web.Application:
    """Erstellt die aiohttp-Applikation mit allen Routen."""
    app = web.Application()
    app["bridge"] = bridge

    # WebSocket (muss vor dem Catch-all kommen)
    app.router.add_get("/ws", ws_handler)

    # Vite-Build Assets: dist/assets → /assets/...
    assets_dir = web_root / "assets"
    if assets_dir.is_dir():
        app.router.add_static("/assets/", path=str(assets_dir), show_index=False)

    # Legacy: web_root unter /static/ (für alte Pfade)
    app.router.add_static("/static/", path=str(web_root), show_index=False)

    # Root → index.html
    async def index_handler(_request: web.Request) -> web.StreamResponse:
        return web.FileResponse(str(web_root / "index.html"))

    app.router.add_get("/", index_handler)

    # SPA-Fallback (z.B. bei Refresh auf /logs, /settings)
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


# ---- Main --------------------------------------------------------------------

async def main():
    ap = argparse.ArgumentParser(description="ESP32 Serial ↔ WebSocket Bridge (standalone)")

    # Serial-Konfiguration
    ap.add_argument("--port", default="auto", help="Gerätepfad oder 'auto' für Autodetect")
    ap.add_argument("--baud", type=int, default=921600, help="Baudrate (Standard: 921600)")
    ap.add_argument("--http", type=int, default=8081, help="HTTP/WS-Port (Standard: 8081)")

    # USB-Autodetect-Filter
    ap.add_argument("--by-id-substr", default="Espressif_USB_JTAG_serial_debug_unit",
                    help="Substring-Match in /dev/serial/by-id (Linux)")
    ap.add_argument("--vid", type=lambda x: int(x, 0), default=None, help="USB Vendor ID (z.B. 0x303A)")
    ap.add_argument("--pid", type=lambda x: int(x, 0), default=None, help="USB Product ID (z.B. 0x1001)")

    # ACK-Protokoll
    ap.add_argument("--ack-timeout", type=float, default=0.2, help="ACK-Timeout in Sekunden")
    ap.add_argument("--retries", type=int, default=3, help="Wiederholungsversuche bei ACK-Timeout")

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

    assets_dir = (web_root / "assets")
    assets_note = "assets=on" if assets_dir.is_dir() else "assets=off"
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
