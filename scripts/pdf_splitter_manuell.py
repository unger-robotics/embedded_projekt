#!/usr/bin/env python3
import os
import sys
from pypdf import PdfReader, PdfWriter

# ==============================================================================
# 1. KONFIGURATION (HIER NEUE PDFs EINTRAGEN)
# ==============================================================================
# Format: "Dateiname.pdf": [ (Startseite, Endseite, "Titel"), ... ]
# Hinweis: Seitenzahlen sind IMMER die PDF-Seiten (wie im Viewer angezeigt, 1-basiert).
#          Das Skript rechnet das intern um.

PDF_REGISTRY = {
    # --- DATEI 1: Das ausführliche Lehrbuch ---
    "10_IV Thermodynamik.pdf": [
        (2,   16,  "Kapitel_13_Temperatur_und_Nullter_Hauptsatz"),
        (17,  41,  "Kapitel_14_Kinetische_Gastheorie"),
        (42,  73,  "Kapitel_15_Waerme_und_Erster_Hauptsatz"),
        (74,  103, "Kapitel_16_Zweiter_Hauptsatz"),
        (104, 119, "Kapitel_17_Waermeuebertragung")
    ],

    # --- DATEI 2: Die Zusammenfassung / Teil IV ---
    "10_Teil IV Thermodynamik.pdf": [
        (2,   5,   "Kapitel_13_Temperatur_und_Nullter_Hauptsatz"),
        (6,   15,  "Kapitel_14_Kinetische_Gastheorie"),
        (16,  27,  "Kapitel_15_Waerme_und_Erster_Hauptsatz"),
        (28,  39,  "Kapitel_16_Zweiter_Hauptsatz"),
        (40,  46,  "Kapitel_17_Waermeuebertragung")
    ],

    # --- PLATZHALTER FÜR NEUE DATEI (Einfach kopieren und ausfüllen) ---
    # "Mein_Neues_Buch.pdf": [
    #     (1, 10, "Kapitel_01_Einleitung"),
    #     (11, 20, "Kapitel_02_Hauptteil")
    # ]
}

# ==============================================================================
# 2. PROGRAMMLOGIK (NICHT ÄNDERN)
# ==============================================================================

def process_single_pdf(filepath, chapters):
    """Verarbeitet eine einzelne PDF-Datei basierend auf der Konfiguration."""
    filename = os.path.basename(filepath)
    base_dir = os.path.dirname(filepath)

    # Erstelle einen sauberen Unterordner für dieses Buch
    # Entferne .pdf Endung für den Ordnernamen
    folder_name = os.path.splitext(filename)[0]
    output_dir = os.path.join(base_dir, "output_split", folder_name)

    print(f"\n--- Starte Verarbeitung: {filename} ---")

    try:
        reader = PdfReader(filepath)
        total_pages = len(reader.pages)
    except Exception as e:
        print(f"❌ Fehler beim Öffnen der Datei: {e}")
        return

    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    success_count = 0

    for i, (start_p, end_p, raw_title) in enumerate(chapters):
        # 1. Validierung
        if start_p > total_pages:
            print(f"   ⚠️  Überspringe '{raw_title}': Startseite {start_p} > PDF-Länge {total_pages}")
            continue

        # Korrigiere das Ende, falls es über das PDF hinausgeht
        real_end = min(end_p, total_pages)

        # 2. Umrechnung (Mensch 1-basiert -> Python 0-basiert)
        idx_start = start_p - 1
        idx_end = real_end # range ist exklusiv, daher passt das genau

        if idx_start >= idx_end:
            print(f"   ⚠️  Ungültiger Bereich für '{raw_title}': {start_p}-{real_end}")
            continue

        # 3. PDF erstellen
        writer = PdfWriter()
        for p in range(idx_start, idx_end):
            writer.add_page(reader.pages[p])

        # Dateinamen formatieren
        fname = f"{i+1:02d}_{raw_title}.pdf"
        out_path = os.path.join(output_dir, fname)

        with open(out_path, "wb") as f:
            writer.write(f)

        print(f"   ✅ [S.{start_p:03d}-{real_end:03d}] -> {fname}")
        success_count += 1

    print(f"-> Abgeschlossen: {success_count} Dateien in '{output_dir}' erstellt.")


def main(target_path):
    """Hauptfunktion: Scannt Ordner und ordnet Dateien der Konfiguration zu."""

    # Falls eine einzelne Datei übergeben wurde
    if os.path.isfile(target_path) and target_path.lower().endswith(".pdf"):
        filename = os.path.basename(target_path)
        if filename in PDF_REGISTRY:
            process_single_pdf(target_path, PDF_REGISTRY[filename])
        else:
            print(f"ℹ️  Datei '{filename}' nicht in der Konfiguration (PDF_REGISTRY) gefunden.")
        return

    # Falls ein Ordner übergeben wurde (Standard)
    if os.path.isdir(target_path):
        found_any = False
        for root, dirs, files in os.walk(target_path):
            if "output_split" in root: continue # Output ignorieren

            for file in files:
                if file in PDF_REGISTRY:
                    found_any = True
                    full_path = os.path.join(root, file)
                    process_single_pdf(full_path, PDF_REGISTRY[file])

        if not found_any:
            print("ℹ️  Keine passenden PDFs im Ordner gefunden, die in der Konfiguration stehen.")
            print("   (Bitte prüfe die Dateinamen in 'PDF_REGISTRY' oben im Skript)")

if __name__ == "__main__":
    target = sys.argv[1] if len(sys.argv) > 1 else "."
    main(target)
