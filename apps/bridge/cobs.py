"""
cobs.py – Consistent Overhead Byte Stuffing.

COBS codiert Daten so, dass keine Null-Bytes im Ergebnis vorkommen.
Damit kann 0x00 als Frame-Delimiter verwendet werden.

Referenz: https://en.wikipedia.org/wiki/Consistent_Overhead_Byte_Stuffing
"""
from __future__ import annotations


def cobs_encode(data: bytes) -> bytes:
    """
    Codiert Daten mit COBS.

    Jedes Null-Byte wird entfernt und durch einen Overhead-Code ersetzt,
    der angibt, wie viele Bytes bis zum nächsten Null (oder Block-Ende) folgen.
    """
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
    """
    Decodiert COBS-codierte Daten.

    Raises:
        ValueError: Bei ungültiger COBS-Codierung (Code=0 oder Overrun)
    """
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

        # Wenn Code != 0xFF, dann folgt ein implizites Null-Byte (außer am Ende)
        if code != 0xFF and idx < len(data):
            out.append(0)

    return bytes(out)
