"""
bridge_core.py – Kernlogik der Bridge.

Verantwortlichkeiten:
- Serial-Verbindung mit automatischem Reconnect
- COBS-Framing und CRC-Validierung
- Protokoll-Handling (HELLO, SELECTION_STATE, ACK)
- WebSocket-Broadcast an verbundene Clients
- setSelection-Kommando mit ACK und Retry

Abhängigkeiten (gleicher Ordner):
- protocol.py: Konstanten + build_packet()/parse_packet()
- cobs.py: cobs_encode()/cobs_decode()
- util.py: log()
- serial_link.py: autodetect_port()/wait_for_port()
"""
from __future__ import annotations

import asyncio
import struct
import time
from typing import Any

import serial_asyncio

try:
    from aiohttp import web
except Exception:  # pragma: no cover
    web = Any  # type: ignore


# ---- Imports aus aufgeteilten Modulen ----------------------------------------

try:
    from protocol import (
        TYPE_HELLO,
        TYPE_SELECTION_STATE,
        TYPE_CMD_SET_SELECTION,
        TYPE_ACK,
        FLAG_ACK_REQ,
        SRC_NAMES,
        build_packet,
        parse_packet,
    )
except Exception as e:  # pragma: no cover
    raise ImportError(
        "protocol.py fehlt oder unvollständig. Benötigt: "
        "TYPE_HELLO, TYPE_SELECTION_STATE, TYPE_CMD_SET_SELECTION, TYPE_ACK, "
        "FLAG_ACK_REQ, SRC_NAMES, build_packet, parse_packet."
    ) from e

try:
    from cobs import cobs_encode, cobs_decode
except Exception as e:  # pragma: no cover
    raise ImportError("cobs.py fehlt oder unvollständig. Benötigt: cobs_encode, cobs_decode.") from e

try:
    from util import log
except Exception as e:  # pragma: no cover
    raise ImportError("util.py fehlt oder unvollständig. Benötigt: log(msg: str) -> None.") from e


# serial_link.py ist optional; bei Fehlen nutzen wir Fallback-Implementierungen.
try:
    from serial_link import autodetect_port, wait_for_port  # type: ignore
except Exception:  # pragma: no cover
    import glob
    import os
    from serial.tools import list_ports

    def autodetect_port(by_id_substr: str | None, vid: int | None, pid: int | None) -> str | None:
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
        Wartet bis ein nutzbarer Port verfügbar ist.
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


# ---- Wire-Framing ------------------------------------------------------------

FRAME_DELIM = b"\x00"  # COBS-Delimiter
MAX_FRAME = 2048       # Maximale Frame-Größe (ohne Delimiter)
MAX_RX_BUF = MAX_FRAME + 1


class Bridge:
    """
    Zentrale Bridge-Klasse.

    Verantwortlichkeiten:
    - Serial-Verbindung mit Reconnect-Logik
    - Framed RX/TX (COBS + CRC)
    - Protokoll-Handling (HELLO/SELECTION/ACK)
    - Kommando: setSelection (mit ACK + Retry)
    - JSON-Broadcast an verbundene WebSocket-Clients
    """

    def __init__(self, ack_timeout_s: float = 0.2, retries: int = 3):
        self.ws_clients: set[web.WebSocketResponse] = set()

        self.reader: asyncio.StreamReader | None = None
        self.writer: asyncio.StreamWriter | None = None

        self.tx_seq = 1
        self.tx_lock = asyncio.Lock()

        # Aus HELLO gelernt
        self.btn_count = 10
        self.led_count = 10

        self.ack_timeout_s = float(ack_timeout_s)
        self.retries = int(retries)
        self.pending_acks: dict[int, asyncio.Future[bool]] = {}

        # Letzter bekannter Zustand für "recover" nach WS-Reconnect
        self.last_selected: int = 0
        self.last_hello: dict[str, Any] | None = None
        self.last_port: str | None = None

    # ---- WebSocket-Client-Verwaltung -----------------------------------------

    def ws_add(self, ws: web.WebSocketResponse) -> None:
        """Fügt einen WebSocket-Client hinzu."""
        self.ws_clients.add(ws)

    def ws_discard(self, ws: web.WebSocketResponse) -> None:
        """Entfernt einen WebSocket-Client."""
        self.ws_clients.discard(ws)

    async def ws_push_recover_state(self, ws: web.WebSocketResponse) -> None:
        """
        Sendet den letzten bekannten Zustand an einen frisch verbundenen WS-Client.
        Aufrufen nach ws.prepare() im WS-Handler.
        """
        if self.last_hello:
            await ws.send_json(self.last_hello)

        await ws.send_json({"type": "selection", "selected": int(self.last_selected), "source": "recover", "tMs": 0})

        if self.last_port:
            await ws.send_json({
                "type": "serial",
                "state": "connected" if self.serial_connected() else "disconnected",
                "port": self.last_port,
            })

    async def broadcast(self, obj: dict[str, Any]) -> None:
        """Sendet ein JSON-Objekt an alle verbundenen WS-Clients."""
        dead: list[web.WebSocketResponse] = []
        for ws in self.ws_clients:
            try:
                await ws.send_json(obj)
            except Exception:
                dead.append(ws)
        for ws in dead:
            self.ws_clients.discard(ws)

    # ---- Serial-Verbindung ---------------------------------------------------

    def serial_connected(self) -> bool:
        """Prüft ob Serial-Verbindung aktiv ist."""
        return self.writer is not None

    async def open_serial(self, port: str, baud: int) -> None:
        """Öffnet die Serial-Verbindung."""
        self.reader, self.writer = await serial_asyncio.open_serial_connection(
            url=port,
            baudrate=int(baud),
            limit=MAX_RX_BUF,  # Wichtig für readuntil()
        )
        self.last_port = port

    async def close_serial(self) -> None:
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

    async def serial_send(self, raw: bytes) -> None:
        """Sendet Rohdaten COBS-codiert über Serial."""
        if self.writer is None:
            raise RuntimeError("serial_not_connected")
        wire = cobs_encode(raw) + FRAME_DELIM
        self.writer.write(wire)
        await self.writer.drain()

    async def serial_reader_once(self) -> None:
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

            try:
                raw = cobs_decode(frame)
                mtype, flags, seq, payload = parse_packet(raw)
            except Exception as e:
                await self.broadcast({"type": "warn", "code": 13, "detail": f"frame_parse_failed: {e}"})
                continue

            await self.handle_serial(mtype, flags, seq, payload)

    async def serial_reader_task(self) -> None:
        """
        Reader-Task: läuft bis Disconnect oder Abbruch.
        """
        assert self.reader is not None

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

    async def handle_serial(self, mtype: int, _flags: int, seq: int, payload: bytes) -> None:
        """Verarbeitet empfangene Serial-Nachrichten nach Typ."""

        # HELLO: ESP32 meldet sich mit Konfiguration
        if mtype == TYPE_HELLO and len(payload) >= 9:
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
                "source": SRC_NAMES[source] if 0 <= source < len(SRC_NAMES) else "unknown",
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

        # Unbekannter Nachrichtentyp
        await self.broadcast({"type": "warn", "code": 10, "detail": f"unknown_msg_type: 0x{mtype:02X}"})

    # ---- Kommandos -----------------------------------------------------------

    async def cmd_set_selection(self, selected: int) -> None:
        """
        Host-Kommando: setSelection (0..btn_count).

        Verwendet ACK mit Retry. Sendet warn/error JSON bei Fehlern.
        selected=0 bedeutet "alle aus" (One-Hot-Regel).
        """
        if not self.serial_connected():
            await self.broadcast({"type": "error", "code": 30, "detail": "serial_not_connected"})
            return

        # Wert auf 0..btn_count begrenzen (0=alle aus)
        try:
            selected = int(selected)
        except Exception:
            selected = 0

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

            last_err: Exception | None = None
            for attempt in range(self.retries + 1):
                fut: asyncio.Future[bool] = asyncio.get_running_loop().create_future()
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
                    last_err = e
                    self.pending_acks.pop(seq, None)
                    await self.broadcast({"type": "error", "code": 22, "detail": f"serial_send_failed: {e}"})
                    return

            await self.broadcast({"type": "error", "code": 21, "detail": f"ack_failed seq={seq}"})
            if last_err:
                raise last_err

    # ---- Verbindungs-Loop ----------------------------------------------------

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
    ) -> None:
        """
        Reconnect-Loop: verbindet sich endlos bis zum Abbruch.

        Bei Verbindungsverlust wird nach reconnect_delay_s automatisch
        ein neuer Verbindungsversuch gestartet.
        """
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
