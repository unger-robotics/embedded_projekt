"""
crc.py – CRC-16/CCITT-FALSE Implementierung.

Parameter:
- Init: 0xFFFF
- Polynom: 0x1021
- RefIn: False
- RefOut: False
- XorOut: 0x0000

Verwendet im Serial-Protokoll zur Integritätsprüfung.
"""
from __future__ import annotations


def crc16_ccitt_false(data: bytes) -> int:
    """
    Berechnet CRC-16/CCITT-FALSE über die gegebenen Daten.

    Returns:
        16-Bit CRC-Wert (0x0000..0xFFFF)
    """
    crc = 0xFFFF
    for b in data:
        crc ^= (b << 8) & 0xFFFF
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc
