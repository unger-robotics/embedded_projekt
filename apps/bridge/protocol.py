"""
protocol.py – Serial-Protokoll-Definitionen.

Definiert:
- Konstanten (Magic, Version, Message-Types, Flags)
- Paket-Aufbau und -Parsing
- Source-Namen für SELECTION_STATE

Protokoll-Spezifikation: docs/PROTOCOL.md (SSOT)
"""
from __future__ import annotations

import struct
from crc import crc16_ccitt_false

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

# Source-Namen für SELECTION_STATE
SRC_NAMES = ["local_btn", "host_cmd", "reset", "recover"]

# ---- Header-Format -----------------------------------------------------------

_HDR_FMT = "<BBBBBBHH"  # 10 Bytes: magic0, magic1, ver_maj, ver_min, type, flags, seq, payload_len
_HDR_LEN = 10
_CRC_LEN = 2


def build_packet(msg_type: int, flags: int, seq: int, payload: bytes) -> bytes:
    """
    Baut ein Paket aus Header + Payload + CRC.

    Struktur:
    - Header (10 Bytes): Magic, Version, Type, Flags, Seq, PayloadLen
    - Payload (variabel)
    - CRC16 (2 Bytes): CCITT-FALSE über Header+Payload
    """
    hdr = struct.pack(
        _HDR_FMT,
        MAGIC0,
        MAGIC1,
        VER_MAJOR,
        0,  # Minor (reserviert)
        msg_type & 0xFF,
        flags & 0xFF,
        seq & 0xFFFF,
        len(payload) & 0xFFFF,
    )
    crc = crc16_ccitt_false(hdr + payload)
    return hdr + payload + struct.pack("<H", crc)


def parse_packet(raw: bytes) -> tuple[int, int, int, bytes]:
    """
    Parst ein empfangenes Paket.

    Prüft:
    - Mindestlänge (Header + CRC)
    - Magic-Bytes
    - Major-Version
    - Payload-Länge
    - CRC

    Returns:
        (msg_type, flags, seq, payload)

    Raises:
        ValueError: Bei ungültigem Paket
    """
    if len(raw) < (_HDR_LEN + _CRC_LEN):
        raise ValueError("zu kurz")

    magic0, magic1, vmaj, _vmin, mtype, flags, seq, ln = struct.unpack(_HDR_FMT, raw[:_HDR_LEN])

    if magic0 != MAGIC0 or magic1 != MAGIC1:
        raise ValueError("falsches Magic")
    if vmaj != VER_MAJOR:
        raise ValueError("falsche Major-Version")

    need = _HDR_LEN + ln + _CRC_LEN
    if len(raw) != need:
        raise ValueError("falsche Länge")

    payload = raw[_HDR_LEN : _HDR_LEN + ln]
    (crc_rx,) = struct.unpack("<H", raw[_HDR_LEN + ln : _HDR_LEN + ln + _CRC_LEN])
    crc_ok = crc16_ccitt_false(raw[: _HDR_LEN + ln])
    if crc_rx != crc_ok:
        raise ValueError("CRC-Fehler")

    return int(mtype), int(flags), int(seq), payload
