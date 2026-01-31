"""
util.py – Hilfsfunktionen.

Stellt einfache Utilities bereit, die von mehreren Modulen genutzt werden.
"""
from __future__ import annotations

import time


def log(msg: str) -> None:
    """
    Gibt eine Log-Nachricht mit Zeitstempel aus.

    Format: YYYY-MM-DD HH:MM:SS [bridge] <nachricht>
    """
    ts = time.strftime("%Y-%m-%d %H:%M:%S")
    print(f"{ts} [bridge] {msg}", flush=True)
