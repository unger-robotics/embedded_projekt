"""
serial_link.py – Serial-Port-Erkennung.

Funktionen:
- autodetect_port(): Findet automatisch den ESP32-Port
- wait_for_port(): Wartet bis ein Port verfügbar ist

Unterstützt:
- Linux: /dev/serial/by-id (stabiler Pfad)
- macOS: /dev/cu.* über list_ports
- Windows: COM-Ports über list_ports
"""
from __future__ import annotations

import asyncio
import glob
import os
import time
from serial.tools import list_ports


def autodetect_port(by_id_substr: str | None, vid: int | None, pid: int | None) -> str | None:
    """
    Erkennt automatisch den Serial-Port des ESP32.

    Suchstrategie:
    1. Linux: /dev/serial/by-id mit Substring-Filter
    2. Plattformübergreifend: list_ports mit optionalem VID/PID-Filter

    Args:
        by_id_substr: Substring für /dev/serial/by-id Matching (Linux)
        vid: USB Vendor ID (z.B. 0x303A für Espressif)
        pid: USB Product ID (z.B. 0x1001)

    Returns:
        Gefundener Port-Pfad oder None
    """
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

    # ttyACM/ttyUSB auf Linux bevorzugen; macOS /dev/cu.* funktioniert auch
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

    Modi:
    - port_arg="auto": Wartet bis autodetect_port() einen Port findet
    - port_arg=Pfad: Wartet bis der angegebene Pfad existiert

    Args:
        port_arg: "auto" oder expliziter Pfad
        by_id_substr: Substring für Autodetect
        vid: USB Vendor ID
        pid: USB Product ID
        poll_s: Poll-Intervall in Sekunden
        timeout_s: Timeout in Sekunden (0=unbegrenzt)

    Returns:
        Gefundener Port-Pfad

    Raises:
        TimeoutError: Wenn Timeout erreicht wird
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
