#!/usr/bin/env bash
# init_bridge_structure.sh – Erstellt die Bridge-Projektstruktur.
#
# Verwendung:
#   ./init_bridge_structure.sh [zielverzeichnis]
#
# Standard-Zielverzeichnis: ./bridge

set -euo pipefail

TARGET_DIR="${1:-bridge}"

echo "Erstelle Bridge-Struktur in: $TARGET_DIR"

mkdir -p "$TARGET_DIR"

# Python-Module (leere Platzhalter)
touch "$TARGET_DIR/bridge.py"
touch "$TARGET_DIR/bridge_core.py"
touch "$TARGET_DIR/web_app.py"
touch "$TARGET_DIR/protocol.py"
touch "$TARGET_DIR/cobs.py"
touch "$TARGET_DIR/crc.py"
touch "$TARGET_DIR/serial_link.py"
touch "$TARGET_DIR/util.py"

# Test-Verzeichnis
mkdir -p "$TARGET_DIR/tests"
touch "$TARGET_DIR/tests/.gitkeep"

# Statische Dateien (Fallback wenn kein Vite-Build)
mkdir -p "$TARGET_DIR/static"
touch "$TARGET_DIR/static/.gitkeep"

# Dist-Verzeichnis (für Vite-Build)
mkdir -p "$TARGET_DIR/dist"
touch "$TARGET_DIR/dist/.gitkeep"

# Packaging-Dateien
touch "$TARGET_DIR/pyproject.toml"
touch "$TARGET_DIR/requirements.txt"

echo "Fertig. Erstellte Dateien:"
find "$TARGET_DIR" -maxdepth 2 -type f | sort
