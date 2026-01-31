# Serial-Protokoll: Robuste Frames mit COBS + CRC

---

## 0) Begriffsrahmen und Zielsetzung

### Anwendungskontext

Der ESP32-S3 kommuniziert mit dem Raspberry Pi über USB-CDC (virtueller Serial-Port). Die Herausforderung: Serielle Kommunikation ist ein kontinuierlicher Byte-Strom ohne inhärente Nachrichtengrenzen. Diese Dokumentation beschreibt das Framing-Protokoll, das zuverlässige, bidirektionale Kommunikation mit Fehlererkennung und Retry-Mechanik ermöglicht.

### Lernziele

Nach dem Durcharbeiten dieses Dokuments kannst du:

1. **Das Framing-Problem erklären** – verstehen, warum ein Byte-Strom Nachrichtengrenzen braucht
2. **COBS anwenden** – das Stuffing-Verfahren von Hand durchrechnen und dekodieren
3. **CRC-16 verstehen** – die Parameter des CCITT-FALSE-Polynoms benennen und die Fehlererkennungsrate einordnen
4. **Das Paket-Layout lesen** – Header-Felder und ihre Funktion erklären
5. **Die ACK/Retry-Mechanik nachvollziehen** – Timing-Parameter und Fehlerfälle beschreiben
6. **Den Datenfluss tracen** – den Weg vom Button-Druck bis zur Browser-Anzeige verfolgen

### Begriffsdefinitionen

| Begriff       | Definition                                                              | Quelle/Norm             |
|---------------|-------------------------------------------------------------------------|-------------------------|
| **USB-CDC**   | USB Communication Device Class – emuliert einen seriellen Port über USB | USB-IF CDC 1.2          |
| **Frame**     | Ein abgegrenztes Datenpaket mit definiertem Anfang und Ende             | Allgemeine Terminologie |
| **COBS**      | Consistent Overhead Byte Stuffing – Framing mit `0x00` als Delimiter    | Cheshire/Baker (1999)   |
| **CRC-16**    | Cyclic Redundancy Check – Prüfsumme zur Fehlererkennung (16 Bit)        | ITU-T V.41, ISO 3309    |
| **Payload**   | Nutzdaten innerhalb eines Frames (ohne Header und CRC)                  | Netzwerk-Terminologie   |
| **ACK**       | Acknowledgment – Empfangsbestätigung für zuverlässige Übertragung       | TCP/IP, ISO 7498        |
| **Delimiter** | Trennzeichen zwischen Frames (hier: `0x00`)                             | Framing-Theorie         |
| **Stuffing**  | Transformation der Daten, um reservierte Zeichen zu eliminieren         | HDLC, PPP, COBS         |

---

## 1) Das Grundproblem – Byte-Strom vs. Nachrichten

### Problemstellung

Wenn du Bytes über eine serielle Leitung schickst, sieht der Empfänger nur einen kontinuierlichen Strom ohne Struktur:

```
Sender:   [Nachricht A: 5 Bytes][Nachricht B: 3 Bytes][Nachricht C: 7 Bytes]
Leitung:  0x12 0x34 0x56 0x78 0x9A 0xBC 0xDE 0xF0 0x11 0x22 0x33 0x44 0x55 0x66 0x77
Empfänger: "Wo fängt Nachricht B an? Wo endet sie?"
```

Ohne Framing weiß der Empfänger nicht, wo eine Nachricht endet und die nächste beginnt. Drei klassische Lösungsansätze existieren:

| Ansatz      | Prinzip                          | Nachteil                                     |
|-------------|----------------------------------|----------------------------------------------|
| Feste Länge | Jede Nachricht hat exakt N Bytes | Unflexibel, verschwendet Bandbreite          |
| Längenfeld  | Erstes Byte gibt Länge an        | Synchronisationsverlust bei Byte-Verlust     |
| Delimiter   | Spezielles Zeichen markiert Ende | Was, wenn das Zeichen in den Daten vorkommt? |

### Herleitung: Warum scheitern die einfachen Ansätze?

**Feste Länge:** Bei einem Selection-Panel variieren die Paketgrößen erheblich – ein `HELLO`-Paket hat 4 Bytes Payload, ein `SNAPSHOT` kann 26 Bytes haben. Feste Länge würde entweder Bandbreite verschwenden oder Flexibilität einschränken.

**Längenfeld:** Angenommen, ein Byte geht verloren:

```
Gesendet:    [Len=5][A][B][C][D][E][Len=3][X][Y][Z]
Empfangen:   [Len=5][A][B][C][D][  ][Len=3][X][Y][Z]
                                  ↑ fehlt
Interpretation: Len=5 → lese A,B,C,D,Len=3 (5 Bytes) → nächstes Paket beginnt bei X
Ergebnis: Paket 1 enthält Müll, Paket 2 ist verschoben, alle weiteren ebenfalls
```

Der Empfänger verliert die Synchronisation und kann sie ohne externes Signal nicht wiedererlangen.

**Delimiter (naiv):** Wählen wir `0x00` als Trennzeichen. Was passiert, wenn die Nutzdaten selbst `0x00` enthalten?

```
Gesendet:    [0x12][0x00][0x34][0x00]   ← Delimiter am Ende
                    ↑
              Dieses 0x00 ist ein Datenbyte!
Empfangen:   [0x12][0x00]   ← Empfänger denkt: "Paket fertig!"
             [0x34][0x00]   ← Zweites (falsches) Paket
```

### Regel

**COBS kombiniert Delimiter-Ansatz mit Byte-Stuffing:** Das Trennzeichen `0x00` kann garantiert nicht in den Daten vorkommen, weil wir die Daten vor dem Senden transformieren.

> **Merkregel:** „Delimiter-Framing braucht Stuffing – sonst verwechselt der Empfänger Daten mit Grenzen."

---

## 2) COBS – Consistent Overhead Byte Stuffing

### Problemstellung

Wir brauchen ein Verfahren, das jedes `0x00` in den Originaldaten eliminiert, aber die Transformation umkehrbar bleibt. Das Verfahren soll effizient sein – möglichst wenig Overhead hinzufügen.

### Kernidee

COBS ersetzt jede `0x00` in den Originaldaten durch einen „Zeiger" auf die nächste Null (oder das Paketende). Das erste Byte des COBS-Blocks ist ebenfalls ein Zeiger.

### Herleitung: Schritt-für-Schritt-Kodierung

**Originaldaten:** `[0x11] [0x22] [0x00] [0x33] [0x00] [0x44]`

Wir identifizieren die Segmente zwischen den Nullen:

```
Segment 1: [0x11] [0x22]       ← 2 Bytes, dann kommt 0x00
Segment 2: [0x33]              ← 1 Byte, dann kommt 0x00
Segment 3: [0x44]              ← 1 Byte, dann kommt Ende
```

Jetzt bauen wir die COBS-Kodierung:

```
Schritt 1: Segment 1 hat 2 Bytes → Zeiger = 2+1 = 3 ("nach 3 Bytes kommt Ersatz")
           [0x03] [0x11] [0x22]

Schritt 2: Segment 2 hat 1 Byte → Zeiger = 1+1 = 2
           [0x02] [0x33]

Schritt 3: Segment 3 hat 1 Byte → Zeiger = 1+1 = 2 (zeigt auf Ende)
           [0x02] [0x44]

Ergebnis:  [0x03] [0x11] [0x22] [0x02] [0x33] [0x02] [0x44]
```

**Visualisierung der Zeiger:**

```
COBS-kodiert: [0x03] [0x11] [0x22] [0x02] [0x33] [0x02] [0x44]
               │                   │            │
               └─ "nach 3 Bytes    └─ "nach 2   └─ "nach 2 Bytes
                  kommt Ersatz"       Bytes"       kommt Ende"
```

### Anwendung: Dekodierung

Der Empfänger dekodiert in umgekehrter Reihenfolge:

```
Eingabe: [0x03] [0x11] [0x22] [0x02] [0x33] [0x02] [0x44]

1. Lies Zeiger: 0x03 → kopiere nächste 2 Bytes, füge 0x00 ein
   Ausgabe bisher: [0x11] [0x22] [0x00]

2. Lies Zeiger: 0x02 → kopiere nächstes 1 Byte, füge 0x00 ein
   Ausgabe bisher: [0x11] [0x22] [0x00] [0x33] [0x00]

3. Lies Zeiger: 0x02 → kopiere nächstes 1 Byte, Ende erreicht
   Ausgabe: [0x11] [0x22] [0x00] [0x33] [0x00] [0x44]
```

### Wire-Frame-Aufbau

Nach der COBS-Kodierung hängen wir den Delimiter `0x00` an:

```
┌─────────────────────────────────────────────────────────┐
│  COBS-kodierte Daten (keine 0x00 enthalten)  │  0x00   │
│                                              │ Delimiter│
└─────────────────────────────────────────────────────────┘
```

### Overhead-Analyse

| Szenario                   | Original | COBS  | Overhead      |
|----------------------------|----------|-------|---------------|
| Keine Nullen in 10 Bytes   | 10       | 11    | +1 Byte (10%) |
| Eine Null in 10 Bytes      | 10       | 11    | +1 Byte (10%) |
| Nur Nullen (10× 0x00)      | 10       | 20    | +10 Bytes     |
| Typisches Paket (20 Bytes) | 20       | 21–22 | +1–2 Bytes    |
| Worst Case (254× 0x00)     | 254      | 255   | +1 Byte       |

> **Merkregel:** „COBS kostet maximal 1 Byte pro 254 Originalbytes – bei typischen Paketen unter 100 Bytes ist der Overhead vernachlässigbar."

### Sonderfall: Zeiger-Überlauf

Was passiert bei einem Segment länger als 254 Bytes ohne `0x00`?

COBS verwendet `0xFF` als „Fortsetzungs-Zeiger": Nach 254 Bytes kommt kein `0x00`, sondern der nächste Zeiger. Für unsere Paketgrößen (max. 268 Bytes) ist dieser Fall selten, aber das Protokoll handhabt ihn korrekt.

### Zusammenfassung

```
                    COBS-Encode
Original ──────────────────────────► Keine 0x00 mehr
         (eliminiert alle Nullen)

                    COBS-Decode
Empfangen ─────────────────────────► Original
          (rekonstruiert Nullen)
```

Der Empfänger sammelt Bytes bis `0x00`, dekodiert alles davor mit COBS und hat das Original-Paket.

---

## 3) CRC-16 – Integritätsprüfung

### Problemstellung

Auch mit korrektem Framing können Bytes auf der Leitung verfälscht werden – durch elektromagnetische Interferenz (EMI), Timing-Probleme oder Buffer-Überläufe. Wir brauchen eine Methode, um solche Fehler zu erkennen.

### Regel

Der Sender berechnet eine Prüfsumme (CRC) über die Daten und hängt sie an. Der Empfänger berechnet dieselbe Prüfsumme und vergleicht. Stimmen die Werte nicht überein, ist das Paket korrupt.

### CRC-16/CCITT-FALSE Parameter

| Parameter | Wert     | Bedeutung                           |
|-----------|----------|-------------------------------------|
| Polynom   | `0x1021` | $x^{16} + x^{12} + x^5 + 1$ (CCITT) |
| Init      | `0xFFFF` | Startwert der Berechnung            |
| XorOut    | `0x0000` | Finaler XOR (hier: keiner)          |
| RefIn     | `false`  | Eingabe-Bits nicht gespiegelt       |
| RefOut    | `false`  | Ausgabe-Bits nicht gespiegelt       |

### Herleitung: Warum CCITT-FALSE?

Dieses Polynom hat mathematisch nachgewiesene Eigenschaften:

| Fehlertyp                        | Erkennungsrate             |
|----------------------------------|----------------------------|
| Alle Einzelbitfehler             | 100%                       |
| Alle Doppelbitfehler             | 100%                       |
| Alle Fehler mit ungerader Anzahl | 100%                       |
| Burst-Fehler ≤ 16 Bit            | 100%                       |
| Zufällige Fehler (statistisch)   | $1 - 2^{-16} ≈ 99{,}998\%$ |

*Analogie:* CRC funktioniert wie eine Quersumme, aber mit Polynomrechnung statt Addition. Ähnlich wie die Quersumme einer ISBN Tippfehler erkennt, erkennt CRC Übertragungsfehler. Die Analogie endet dort, wo es um Burst-Fehler geht: Eine einfache Quersumme erkennt keine Vertauschungen, CRC schon.

### Anwendung: Referenz-Implementierung (Python)

```python
def crc16_ccitt_false(data: bytes) -> int:
    """
    Berechnet CRC-16/CCITT-FALSE.

    Args:
        data: Zu prüfende Bytes

    Returns:
        16-Bit CRC-Wert (0x0000–0xFFFF)
    """
    crc = 0xFFFF  # Init
    for b in data:
        crc ^= (b << 8) & 0xFFFF
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc  # XorOut = 0x0000, also keine Änderung
```

**Zahlenbeispiel:**

```python
>>> crc16_ccitt_false(b"123456789")
0x29B1

>>> crc16_ccitt_false(b"\x53\x50\x01\x00\x12\x00\x00\x00\x08\x00")
0x3A7E  # Header eines SELECTION_STATE-Pakets
```

### Byte-Reihenfolge im Paket

Der CRC wird als Little-Endian angehängt (Low-Byte zuerst):

```
CRC = 0x3A7E → Paket: [...] [0x7E] [0x3A]
                            Low    High
```

### Zusammenfassung

> **Merkregel:** „CRC ist keine Verschlüsselung – es erkennt Fehler, verhindert sie aber nicht."

Wenn der CRC nicht stimmt, verwirft der Empfänger das Paket. Der Sender muss es erneut senden (siehe ACK-Mechanik in Abschnitt 7).

---

## 4) Packet-Layout – Der Header

### Problemstellung

Bevor wir COBS anwenden, müssen wir wissen, wie das „Raw-Packet" aussieht. Welche Metadaten brauchen wir zusätzlich zu den Nutzdaten?

### Header-Struktur (10 Bytes)

```
┌────────┬────────────┬───────┬─────────────────────────────────────────┐
│ Offset │ Feld       │ Größe │ Beschreibung                            │
├────────┼────────────┼───────┼─────────────────────────────────────────┤
│ 0      │ magic0     │ 1     │ 'S' (0x53) – erstes Magic-Byte          │
│ 1      │ magic1     │ 1     │ 'P' (0x50) – zweites Magic-Byte         │
│ 2      │ ver_major  │ 1     │ Major-Version (aktuell: 1)              │
│ 3      │ ver_minor  │ 1     │ Minor-Version (aktuell: 0)              │
│ 4      │ type       │ 1     │ Message-Typ (siehe Abschnitt 6)         │
│ 5      │ flags      │ 1     │ Bitflags für ACK, Error, etc.           │
│ 6–7    │ seq        │ 2     │ Sequenznummer (Little-Endian, 1–65535)  │
│ 8–9    │ len        │ 2     │ Payload-Länge in Bytes (Little-Endian)  │
└────────┴────────────┴───────┴─────────────────────────────────────────┘
```

### Gesamtpaket-Aufbau

```
┌──────────────────────────────────────────────────────────────────────┐
│  Header (10 Bytes)  │  Payload (0–256 Bytes)  │  CRC-16 (2 Bytes)    │
└──────────────────────────────────────────────────────────────────────┘
│◄──────────────────── CRC-Berechnung ───────────────────────►│
```

### Herleitung: Warum diese Felder?

**Magic Bytes (`SP` = 0x53 0x50):**

Wenn wir mitten in einem Datenstrom einsteigen oder ein Frame korrupt ist, erkennen wir sofort, dass etwas nicht stimmt.

Die Wahrscheinlichkeit, dass zufällige Daten mit `SP` beginnen:

$$P(\text{zufällig SP}) = \frac{1}{256} \times \frac{1}{256} = \frac{1}{65536} \approx 0{,}0015\%$$

**Version (ver_major, ver_minor):**

Ermöglicht Protokoll-Evolution ohne Inkompatibilität.

| Empfangene Version    | Eigene Version | Aktion                          |
|-----------------------|----------------|---------------------------------|
| Major stimmt          | –              | Akzeptieren                     |
| Major unterschiedlich | –              | Verwerfen (inkompatibel)        |
| Minor höher           | –              | Akzeptieren (abwärtskompatibel) |

**Sequenznummer (seq):**

Identifiziert Pakete eindeutig für die ACK-Zuordnung.

- Bereich: 1–65535 (Wert 0 wird übersprungen)
- Bei Überlauf: 65535 → 1 (nicht 0)
- Jeder Sender hat seinen eigenen Zähler

**Flags-Byte:**

| Bit | Maske | Name      | Bedeutung                         |
|-----|-------|-----------|-----------------------------------|
| 0   | 0x01  | `ACK_REQ` | Sender erwartet Bestätigung       |
| 1   | 0x02  | `IS_ACK`  | Dieses Paket ist eine Bestätigung |
| 2   | 0x04  | `IS_ERR`  | Dieses Paket meldet einen Fehler  |
| 3–7 | –     | Reserved  | Für zukünftige Erweiterungen      |

### Anwendung: Beispiel-Header (hexadezimal)

```
SELECTION_STATE-Paket, seq=1234, len=8:

53 50 01 00 12 01 D2 04 08 00
│  │  │  │  │  │  └──┴──────── len = 0x0008 (8 Bytes)
│  │  │  │  │  │  └──┴──────── seq = 0x04D2 (1234)
│  │  │  │  │  └────────────── flags = 0x01 (ACK_REQ)
│  │  │  │  └─────────────────  type = 0x12 (SELECTION_STATE)
│  │  │  └────────────────────  ver_minor = 0
│  │  └───────────────────────  ver_major = 1
│  └──────────────────────────  magic1 = 'P'
└─────────────────────────────  magic0 = 'S'
```

### Zusammenfassung

> **Merkregel:** „Magic prüft Synchronisation, Version prüft Kompatibilität, CRC prüft Integrität."

---

## 5) Sende- und Empfangsprozess

### Sendeprozess (4 Schritte)

```
Schritt 1: Header + Payload zusammenbauen
┌────────────────────────────────────┐
│  Header (10 Bytes)  │  Payload     │
└────────────────────────────────────┘

Schritt 2: CRC-16 über alles berechnen und anhängen (Little-Endian)
┌────────────────────────────────────┬───────┐
│  Header (10 Bytes)  │  Payload     │ CRC16 │
└────────────────────────────────────┴───────┘

Schritt 3: COBS-encode (eliminiert alle 0x00)
┌──────────────────────────────────────────────┐
│  COBS-kodierte Daten (keine 0x00 enthalten)  │
└──────────────────────────────────────────────┘

Schritt 4: Delimiter anhängen und über Serial senden
┌──────────────────────────────────────────────┬──────┐
│  COBS-kodierte Daten                         │ 0x00 │
└──────────────────────────────────────────────┴──────┘
```

### Empfangsprozess (4 Schritte)

```
Schritt 1: Bytes sammeln bis 0x00 (Delimiter)
┌──────────────────────────────────────────────┬──────┐
│  Empfangene Bytes                            │ 0x00 │
└──────────────────────────────────────────────┴──────┘

Schritt 2: COBS-decode (rekonstruiert Original)
┌────────────────────────────────────┬───────┐
│  Header + Payload                  │ CRC16 │
└────────────────────────────────────┴───────┘

Schritt 3: CRC prüfen
- Berechne CRC über Header + Payload
- Vergleiche mit empfangenen CRC-Bytes
- Bei Mismatch: Paket verwerfen

Schritt 4: Header validieren und Payload extrahieren
- Magic = "SP"? Sonst verwerfen
- Version kompatibel? Sonst verwerfen
- len plausibel? Sonst verwerfen
- Dispatch nach type
```

### Zustandsautomat des Empfängers

```
                    ┌─────────────┐
                    │   IDLE      │
                    └──────┬──────┘
                           │ Byte empfangen (≠ 0x00)
                           ▼
                    ┌─────────────┐
              ┌────►│ COLLECTING  │◄────┐
              │     └──────┬──────┘     │
              │            │            │
    Byte ≠ 0x00            │ 0x00       │ Byte ≠ 0x00
              │            ▼            │
              │     ┌─────────────┐     │
              └─────│  DECODING   │─────┘
                    └──────┬──────┘
                           │ Dekodierung + Validierung
                           ▼
              ┌────────────┴────────────┐
              │                         │
        CRC OK                    CRC Fehler
              │                         │
              ▼                         ▼
       ┌──────────┐              ┌──────────┐
       │ DISPATCH │              │  DISCARD │
       └──────────┘              └──────────┘
```

### Zusammenfassung

> **Merkregel:** „Senden = Build → CRC → COBS → Delimiter. Empfangen = Collect → COBS → CRC → Dispatch."

---

## 6) Message-Typen

### Übersicht der Typen nach Richtung

**ESP32 → Pi (Events):**

| Type            | Hex  | Zweck                         | Payload-Größe |
|-----------------|------|-------------------------------|---------------|
| HELLO           | 0x01 | Fähigkeiten nach Reset melden | 4 Bytes       |
| BTN_EVENT       | 0x10 | Button gedrückt/losgelassen   | 8 Bytes       |
| SNAPSHOT        | 0x11 | Kompletter Button/LED-Zustand | variabel      |
| SELECTION_STATE | 0x12 | Aktuell gewählter Kanal       | 8 Bytes       |

**Pi → ESP32 (Commands):**

| Type              | Hex  | Zweck                      | Payload-Größe |
|-------------------|------|----------------------------|---------------|
| HELLO_ACK         | 0x02 | HELLO bestätigen           | 0 Bytes       |
| CMD_SET_SELECTION | 0x20 | Kanal vom Dashboard setzen | 4 Bytes       |
| CMD_REQ_SNAPSHOT  | 0x23 | Debug-Snapshot anfordern   | 0 Bytes       |

**Bidirektional (Control):**

| Type | Hex  | Zweck               | Payload-Größe |
|------|------|---------------------|---------------|
| ACK  | 0x30 | Empfangsbestätigung | 0 Bytes       |
| ERR  | 0x50 | Fehler melden       | variabel      |
| PING | 0x03 | Keepalive-Anfrage   | 0 Bytes       |
| PONG | 0x04 | Keepalive-Antwort   | 0 Bytes       |

### SELECTION_STATE – Das zentrale Event

Dieses Paket synchronisiert den Zustand zwischen ESP32, Pi und Browser. Es wird bei jeder Änderung der Auswahl gesendet.

**Payload-Struktur (8 Bytes):**

```
┌────────┬──────────────────┬───────┬──────────────────────────────────┐
│ Offset │ Feld             │ Typ   │ Beschreibung                     │
├────────┼──────────────────┼───────┼──────────────────────────────────┤
│ 0–1    │ selected_id      │ u16   │ 0 = keiner, 1–100 = Kanal-Nr.    │
│ 2      │ source           │ u8    │ Auslöser der Änderung            │
│ 3      │ reserved         │ u8    │ 0x00 (für Erweiterungen)         │
│ 4–7    │ t_ms             │ u32   │ millis() seit ESP32-Boot         │
└────────┴──────────────────┴───────┴──────────────────────────────────┘
```

**Source-Werte:**

| Wert | Name        | Bedeutung                                 |
|------|-------------|-------------------------------------------|
| 0    | `local_btn` | Physischer Button am Panel gedrückt       |
| 1    | `host_cmd`  | Dashboard-Klick (via Pi → ESP32)          |
| 2    | `reset`     | Nach ESP32-Neustart (Initialzustand)      |
| 3    | `recover`   | Wiederherstellung nach Verbindungsabbruch |

### Zahlenbeispiel: Komplettes SELECTION_STATE-Paket

```
Szenario: Button 7 gedrückt, millis()=45678

Payload:
  selected_id = 7     → 0x07 0x00 (Little-Endian)
  source = 0          → 0x00
  reserved = 0        → 0x00
  t_ms = 45678        → 0x6E 0xB2 0x00 0x00 (Little-Endian)

  Payload-Bytes: 07 00 00 00 6E B2 00 00

Header:
  magic = "SP"        → 53 50
  version = 1.0       → 01 00
  type = 0x12         → 12
  flags = ACK_REQ     → 01
  seq = 1234          → D2 04 (Little-Endian)
  len = 8             → 08 00 (Little-Endian)

  Header-Bytes: 53 50 01 00 12 01 D2 04 08 00

Raw-Paket (vor CRC): 53 50 01 00 12 01 D2 04 08 00 07 00 00 00 6E B2 00 00
CRC-16 über Raw:     → 0xABCD (Beispiel)
Mit CRC:             53 50 01 00 12 01 D2 04 08 00 07 00 00 00 6E B2 00 00 CD AB

Nach COBS + Delimiter: [COBS-kodiert] 00
```

---

## 7) ACK/Retry-Mechanik

### Problemstellung

Bei Commands wie `CMD_SET_SELECTION` muss der Pi wissen, ob der ESP32 das Kommando erhalten und verarbeitet hat. Ohne Bestätigung könnte ein verlorenes Paket unbemerkt bleiben.

### Regel

Request-Response mit Timeout und Retry: Der Sender wartet auf ACK. Kommt keines, wiederholt er das Paket.

### Erfolgreicher Ablauf

```
Pi                                      ESP32
 │                                        │
 │─── CMD_SET_SELECTION ─────────────────►│  seq=42, flags=ACK_REQ
 │         (Timer startet: 600ms)         │
 │                                        │  (verarbeitet Kommando)
 │◄────────── ACK ────────────────────────│  seq=42, flags=IS_ACK
 │         (Timer stoppt, Erfolg)         │
 │                                        │
 │◄── SELECTION_STATE ────────────────────│  selected_id=7, source=host_cmd
 │                                        │
```

### Fehlerfall mit Retry

```
Pi                                      ESP32
 │                                        │
 │─── CMD_SET_SELECTION ─────────────────►│  seq=43, ACK_REQ
 │         (Timer: 600ms)                 │
 │              ...                       │  (Paket verloren / ESP32 busy)
 │         (Timeout!)                     │
 │                                        │
 │─── CMD_SET_SELECTION ─────────────────►│  seq=43, Retry #1
 │         (Timer: 600ms)                 │
 │              ...                       │  (noch immer keine Antwort)
 │         (Timeout!)                     │
 │                                        │
 │─── CMD_SET_SELECTION ─────────────────►│  seq=43, Retry #2
 │              ...                       │
 │◄────────── ACK ────────────────────────│  (endlich!)
 │                                        │
```

### Timing-Parameter

| Parameter        | Default | Konfiguration in bridge.py |
|------------------|---------|----------------------------|
| ACK-Timeout      | 600 ms  | `--ack-timeout 0.6`        |
| Maximale Retries | 4       | `--retries 4`              |
| Gesamtwartezeit  | 3000 ms | 5 × 600 ms                 |

**Berechnung der Gesamtwartezeit:**

$$T_{\text{max}} = (1 + \text{retries}) \times \text{timeout} = 5 \times 600\,\text{ms} = 3000\,\text{ms}$$

### Fehlerbehandlung nach Retry-Erschöpfung

Nach 5 fehlgeschlagenen Versuchen (1 Original + 4 Retries) meldet die Bridge einen Fehler an den Browser:

```json
{
  "type": "error",
  "code": 21,
  "message": "ACK timeout after 5 attempts",
  "seq": 43
}
```

### Idempotenz

Die Sequenznummer ermöglicht Idempotenz: Wenn der ESP32 dasselbe Paket (gleiche `seq`) mehrfach empfängt, führt er das Kommando nur einmal aus und sendet das ACK erneut.

### Zusammenfassung

> **Merkregel:** „Kein ACK, kein Erfolg. Retry bis zum Limit, dann Error."

---

## 8) Datenfluss – Vom Button zum Browser

### Problemstellung

Die Kommunikationskette hat mehrere Stufen mit unterschiedlichen Protokollen. Wie fließen die Daten konkret vom physischen Button bis zur Browser-Anzeige?

### End-to-End-Datenfluss

```
┌─────────────────────────────────────────────────────────────────────────┐
│ 1. ESP32: CD4021BE liest Button-Matrix (alle 10 ms)                     │
│    Schieberegister → rawButtons[] → Debounce (50 ms Schwellwert)        │
│    → Stabile Flanke erkannt: Button 7 gedrückt                          │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│ 2. ESP32: Baut SELECTION_STATE-Paket                                    │
│    Header: magic=SP, type=0x12, seq=1234, len=8, flags=ACK_REQ          │
│    Payload: selected_id=7, source=0 (local_btn), t_ms=45678             │
│    → CRC-16 berechnen und anhängen                                      │
│    → COBS-encode, 0x00-Delimiter anhängen                               │
│    → Über USB-CDC senden                                                │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                              USB-CDC Serial
                              (/dev/ttyACM0)
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│ 3. Pi (bridge.py): Empfängt Frame                                       │
│    → Bytes sammeln bis 0x00                                             │
│    → COBS-decode                                                        │
│    → CRC prüfen (Mismatch → verwerfen)                                  │
│    → Header parsen, type=0x12 → SELECTION_STATE erkannt                 │
│    → ACK senden (seq=1234, IS_ACK)                                      │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│ 4. Pi (bridge.py): Wandelt in JSON für WebSocket                        │
│    {                                                                    │
│      "type": "selection",                                               │
│      "selected": 7,                                                     │
│      "source": "local_btn",                                             │
│      "tMs": 45678                                                       │
│    }                                                                    │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                              WebSocket :8081/ws
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│ 5. Browser (React): useBridgeWs() empfängt JSON                         │
│    → State-Update: setSelectedChannel(7)                                │
│    → Re-Render: LED 7 leuchtet grün im Dashboard                        │
└─────────────────────────────────────────────────────────────────────────┘
```

### Latenz-Analyse

| Segment                     | Typische Latenz | Einflussfaktoren             |
|-----------------------------|-----------------|------------------------------|
| Button → Debounce-Erkennung | 30–50 ms        | Debounce-Schwellwert         |
| ESP32 → Serial → Pi         | 1–5 ms          | USB-CDC, Pufferung           |
| Pi → WebSocket → Browser    | 5–20 ms         | Netzwerk, Browser-Event-Loop |
| **Gesamt (typisch)**        | **40–75 ms**    |                              |

### Rückrichtung: Dashboard → LED

```
Browser                Pi                    ESP32
   │                    │                      │
   │── WS: select(7) ──►│                      │
   │                    │── CMD_SET_SELECTION ►│
   │                    │     (seq=99)         │
   │                    │◄──── ACK ────────────│
   │                    │                      │  (setzt LED 7)
   │                    │◄── SELECTION_STATE ──│
   │◄── WS: confirm ────│     (source=host_cmd)│
   │                    │                      │
```

### Zusammenfassung

> **Merkregel:** „Button → Serial → Bridge → WebSocket → Browser. Fünf Stufen, eine Wahrheit."

---

## 9) Größenrahmen und Limits

### Konstanten

| Konstante        | Wert       | Herleitung                         |
|------------------|------------|------------------------------------|
| `HEADER_LEN`     | 10 Bytes   | Summe aller Header-Felder          |
| `CRC_LEN`        | 2 Bytes    | CRC-16                             |
| `MAX_PAYLOAD`    | 256 Bytes  | Firmware-Limit (RAM-Beschränkung)  |
| `MAX_PACKET`     | 268 Bytes  | 10 + 256 + 2                       |
| `MAX_WIRE_FRAME` | ~270 Bytes | Nach COBS (max. +1 pro 254) + 0x00 |

### Paketgrößen in der Praxis

| Pakettyp          | Header | Payload | CRC | Raw | Wire (COBS) |
|-------------------|--------|---------|-----|-----|-------------|
| ACK               | 10     | 0       | 2   | 12  | ~13         |
| PING/PONG         | 10     | 0       | 2   | 12  | ~13         |
| SELECTION_STATE   | 10     | 8       | 2   | 20  | ~21–22      |
| SNAPSHOT (10 Ch)  | 10     | 26      | 2   | 38  | ~39–40      |
| SNAPSHOT (100 Ch) | 10     | 206     | 2   | 218 | ~220        |

### Bandbreiten-Betrachtung

Bei 115200 Baud (typische USB-CDC-Einstellung):

| Metrik                      | Wert                                  |
|-----------------------------|---------------------------------------|
| Rohe Bitrate                | 115200 Bit/s                          |
| Effektive Byterate          | 11520 Byte/s (10 Bits pro Byte)       |
| SELECTION_STATE pro Sekunde | ~520 Pakete/s (bei 22 Bytes/Paket)    |
| Praktische Nutzung          | 10–100 Pakete/s (ausreichend Reserve) |

### Zusammenfassung

> **Merkregel:** „Max. 268 Bytes Raw, ~270 Bytes on-Wire. Für unsere Paketgrößen ist Bandbreite kein Engpass."

---

## 10) Fehlerbehandlung

### Fehlerarten und Reaktionen

| Fehlerart            | Erkennung                      | Reaktion                        |
|----------------------|--------------------------------|---------------------------------|
| CRC-Mismatch         | CRC-Prüfung nach COBS-Decode   | Frame verwerfen, kein ACK       |
| Magic falsch         | Byte 0–1 ≠ 0x53 0x50           | Frame verwerfen, Resync         |
| Version inkompatibel | ver_major stimmt nicht         | Frame verwerfen                 |
| Länge inkonsistent   | len vs. tatsächliche Größe     | Frame verwerfen                 |
| ACK-Timeout          | Timer nach Timeout abgelaufen  | Retry, dann Error an Browser    |
| Unbekannter Type     | type nicht in Dispatch-Tabelle | Ignorieren (forward-compatible) |

### Resynchronisation

Da `0x00` garantiert nur als Delimiter vorkommt (dank COBS), findet der Empfänger nach einem korrupten Frame automatisch den nächsten gültigen Frame-Start:

```
Korrupter Frame        Delimiter   Nächster gültiger Frame
[...Müll...] ──────────► 0x00 ────► [COBS-Daten...] 0x00
                           ↑
                    Hier synchronisiert
                    der Empfänger neu
```

### Error-Paket (Type 0x50)

Bei schwerwiegenden Fehlern sendet eine Seite ein ERR-Paket:

```
┌────────┬──────────────────┬───────┬─────────────────────────────────┐
│ Offset │ Feld             │ Typ   │ Beschreibung                    │
├────────┼──────────────────┼───────┼─────────────────────────────────┤
│ 0      │ error_code       │ u8    │ Fehlercode (siehe Tabelle)      │
│ 1      │ context_type     │ u8    │ Type des auslösenden Pakets     │
│ 2–3    │ context_seq      │ u16   │ Seq des auslösenden Pakets      │
└────────┴──────────────────┴───────┴─────────────────────────────────┘
```

**Fehlercodes:**

| Code | Name               | Bedeutung                       |
|------|--------------------|---------------------------------|
| 1    | `ERR_UNKNOWN_TYPE` | Unbekannter Message-Type        |
| 10   | `ERR_CRC_FAIL`     | CRC-Prüfung fehlgeschlagen      |
| 11   | `ERR_DECODE_FAIL`  | COBS-Dekodierung fehlgeschlagen |
| 20   | `ERR_TIMEOUT`      | Internes Timeout                |
| 21   | `ERR_ACK_TIMEOUT`  | ACK nicht erhalten              |

### Zusammenfassung

> **Merkregel:** „Delimiter-Framing = automatische Resync. Unbekannte Types ignorieren = Forward-Kompatibilität."
