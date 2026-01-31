# Algorithmen

Algorithmische Spezifikationen fuer das Selection Panel Firmware.

---

## Grundprinzip

**Embedded-Algorithmen** sind terminierende Schritte pro Zyklus/Event:

- `loop()` / `io_task()` = Endlosschleife (Systembetrieb)
- Einzelne Operationen darin = Algorithmen (terminierend)

**Format:** Input → Verarbeitung → Output

---

## 1) Debounce-Algorithmus

### Zweck

Entprellung von mechanischen Tastern. Filtert Prellimpulse und erkennt stabile Flanken.

### Parameter

| Parameter     | Wert   | Beschreibung                |
|---------------|--------|-----------------------------|
| `debounce_ms` | 30 ms  | Stabilisierungszeit         |
| `scan_period` | 1 ms   | Abtastintervall             |
| Threshold     | 30     | Samples fuer stabilen Zustand |

### Datenstrukturen

```c
typedef struct debounce_cfg_t {
    uint32_t debounce_ms;  // Stabilisierungszeit
    uint8_t count;         // Anzahl Buttons
} debounce_cfg_t;

// Interner Zustand pro Button
uint8_t _last_raw_pressed[BTN_BYTES];   // Letzter Roh-Zustand
uint8_t _stable_pressed[BTN_BYTES];     // Stabiler Zustand
uint32_t _last_change_ms[BTN_COUNT];    // Zeitstempel letzte Aenderung
```

### Algorithmus (Pseudocode)

```
FUNCTION debounce_update(raw[], now_ms):
    FOR id = 1 TO count:
        byte_idx = (id - 1) / 8
        mask = 1 << (7 - ((id - 1) % 8))

        raw_pressed = (raw[byte_idx] & mask) == 0  // Active-Low
        last_pressed = (_last_raw_pressed[byte_idx] & mask) != 0

        IF raw_pressed != last_pressed:
            // Zustand geaendert → Timer neu starten
            _last_change_ms[id-1] = now_ms

            IF raw_pressed:
                _last_raw_pressed[byte_idx] |= mask
            ELSE:
                _last_raw_pressed[byte_idx] &= ~mask
        ELSE:
            // Zustand stabil → pruefen ob Threshold erreicht
            dt = now_ms - _last_change_ms[id-1]

            IF dt >= debounce_ms:
                stable_was = (_stable_pressed[byte_idx] & mask) != 0

                IF raw_pressed AND NOT stable_was:
                    // Stabile 0→1 Flanke erkannt
                    _stable_pressed[byte_idx] |= mask
                    pressed_edge[byte_idx] |= mask  // OUTPUT

                ELSE IF NOT raw_pressed AND stable_was:
                    // Stabile 1→0 Flanke
                    _stable_pressed[byte_idx] &= ~mask
```

### Zustandsdiagramm

```
                    raw != last
        ┌─────────────────────────────┐
        │                             │
        ▼                             │
   ┌─────────┐                   ┌────┴────┐
   │  IDLE   │◄──────────────────│ PENDING │
   │(stable) │   dt >= 30ms      │(warten) │
   └────┬────┘                   └────┬────┘
        │                             ▲
        │ raw != stable               │ raw == last
        │                             │ dt < 30ms
        └─────────────────────────────┘
```

### Komplexitaet

- Zeit: O(n) pro Aufruf, n = Anzahl Buttons
- Speicher: O(n) fuer Zustandspuffer

---

## 2) Selection-Algorithmus

### Zweck

Waehlt aus mehreren gleichzeitig gedrueckten Buttons einen aus (One-Hot).

### Policies

| Policy                         | Verhalten                      |
|--------------------------------|--------------------------------|
| `SELECTION_POLICY_LOWEST_ID_WINS` | Kleinste ID gewinnt         |
| `SELECTION_POLICY_LAST_ID_WINS`   | Groesste ID gewinnt         |

### Datenstrukturen

```c
typedef enum selection_policy_e {
    SELECTION_POLICY_LOWEST_ID_WINS = 0,
    SELECTION_POLICY_LAST_ID_WINS = 1,
} selection_policy_e;

typedef struct selection_state_t {
    uint8_t selected_id;  // 0 = keine, 1..N = aktiv
} selection_state_t;

typedef struct selection_cfg_t {
    uint8_t count;
    selection_policy_e policy;
} selection_cfg_t;
```

### Algorithmus: selection_pick()

```
FUNCTION selection_pick(edges[], n, cfg) -> uint8_t:
    candidate = 0

    FOR id = 1 TO cfg.count:
        byte_idx = (id - 1) / 8
        mask = 1 << (7 - ((id - 1) % 8))

        IF (edges[byte_idx] & mask) != 0:
            // Pressed-Edge fuer diese ID

            IF cfg.policy == LOWEST_ID_WINS:
                RETURN id  // Sofort zurueck (erste gefundene)
            ELSE:
                candidate = id  // Merken, weitermachen

    RETURN candidate  // 0 wenn keine Edge
```

### Algorithmus: selection_update()

```
FUNCTION selection_update(state, edges[], n, cfg, out_id) -> bool:
    picked = selection_pick(edges, n, cfg)

    IF picked != 0 AND picked != state.selected_id:
        state.selected_id = picked
        *out_id = picked
        RETURN true  // Aenderung

    RETURN false  // Keine Aenderung
```

### Beispiel

```
Eingabe: edges[] = 0x28 0x00  (Bits 5 und 3 gesetzt = ID 3 und 5)

LOWEST_ID_WINS:
  id=1: mask=0x80, edges[0] & 0x80 = 0 → weiter
  id=2: mask=0x40, edges[0] & 0x40 = 0 → weiter
  id=3: mask=0x20, edges[0] & 0x20 = 0x20 → RETURN 3

LAST_ID_WINS:
  id=1..2: keine Edge
  id=3: candidate = 3
  id=4: keine Edge
  id=5: mask=0x08, candidate = 5
  id=6..10: keine Edge
  RETURN 5
```

### Komplexitaet

- Zeit: O(n), Worst-Case durchlaeuft alle IDs
- Speicher: O(1)

---

## 3) Press-Guard-Algorithmus

### Zweck

Verhindert versehentliche Doppel-Druecke derselben Taste innerhalb kurzer Zeit.

### Parameter

| Parameter        | Wert  | Beschreibung                    |
|------------------|-------|---------------------------------|
| `press_guard_ms` | 30 ms | Sperrzeit nach akzeptiertem Druck |

### Datenstrukturen

```c
bool _guard_armed[BTN_COUNT];           // Guard aktiv?
uint32_t _last_accepted_press_ms[BTN_COUNT];  // Zeitstempel
```

### Algorithmus

```
FUNCTION apply_press_guard(edges[], now_ms):
    IF press_guard_ms == 0:
        RETURN  // Guard deaktiviert

    FOR id = 1 TO count:
        byte_idx = (id - 1) / 8
        mask = 1 << (7 - ((id - 1) % 8))

        IF (edges[byte_idx] & mask) == 0:
            CONTINUE  // Keine Edge fuer diese ID

        IF _guard_armed[id-1]:
            dt = now_ms - _last_accepted_press_ms[id-1]

            IF dt < press_guard_ms:
                // Zu frueh → Edge filtern
                edges[byte_idx] &= ~mask
                CONTINUE

        // Edge akzeptiert
        _last_accepted_press_ms[id-1] = now_ms
        _guard_armed[id-1] = true
```

### Zeitdiagramm

```
Button 3:
     ▼ Press    ▼ Press (ignoriert)    ▼ Press (akzeptiert)
─────┴──────────┴──────────────────────┴─────────────────────►
     │◄── 15ms ─►│                      │
     │           │                      │
     │◄────────── 30ms Guard ──────────►│

Ergebnis: Nur erster und dritter Press werden akzeptiert
```

### Komplexitaet

- Zeit: O(n)
- Speicher: O(n)

---

## 4) Bit-Mapping-Algorithmus

### Zweck

Konvertiert zwischen Button/LED-IDs (1-basiert) und Bit-Positionen (MSB-first).

### Formeln

```c
// ID → Byte-Index und Bit-Maske
byte_idx = (id - 1) / 8
mask = 1 << (7 - ((id - 1) % 8))

// Bit setzen
buffer[byte_idx] |= mask

// Bit loeschen
buffer[byte_idx] &= ~mask

// Bit pruefen
is_set = (buffer[byte_idx] & mask) != 0
```

### Mapping-Tabelle

| ID | byte_idx | bit | mask |
|----|----------|-----|------|
| 1  | 0        | 7   | 0x80 |
| 2  | 0        | 6   | 0x40 |
| 3  | 0        | 5   | 0x20 |
| 4  | 0        | 4   | 0x10 |
| 5  | 0        | 3   | 0x08 |
| 6  | 0        | 2   | 0x04 |
| 7  | 0        | 1   | 0x02 |
| 8  | 0        | 0   | 0x01 |
| 9  | 1        | 7   | 0x80 |
| 10 | 1        | 6   | 0x40 |

### Implementierung

```c
static inline uint8_t mask_msb(uint8_t id) {
    return (uint8_t)(1u << (7u - ((id - 1u) % 8u)));
}

static inline uint8_t bytes_for(uint8_t count) {
    return (uint8_t)((count + 7u) / 8u);
}
```

---

## 5) One-Hot-Encoding

### Zweck

Konvertiert selected_id in Bit-Muster fuer LED-Output (genau eine LED oder keine).

### Algorithmus

```
FUNCTION set_one_hot(selected_id, out[], n):
    // Alle Bits loeschen
    FOR i = 0 TO n-1:
        out[i] = 0x00

    IF selected_id == 0:
        RETURN  // Keine LED an

    IF selected_id > count:
        RETURN  // Ungueltige ID

    // Einzelnes Bit setzen
    byte_idx = (selected_id - 1) / 8
    mask = 1 << (7 - ((selected_id - 1) % 8))
    out[byte_idx] = mask
```

### Beispiele

| selected_id | out[0] | out[1] | Hex       |
|-------------|--------|--------|-----------|
| 0           | 0x00   | 0x00   | 0x00 0x00 |
| 1           | 0x80   | 0x00   | 0x80 0x00 |
| 3           | 0x20   | 0x00   | 0x20 0x00 |
| 8           | 0x01   | 0x00   | 0x01 0x00 |
| 10          | 0x00   | 0x40   | 0x00 0x40 |

### Invariante

```
popcount(out[0..n-1]) ∈ {0, 1}  // Immer 0 oder 1 Bit gesetzt
```

---

## 6) Active-Low/High Konvertierung

### Button-Input (CD4021, Active-Low)

```
Hardware:     Bit = 0 → Button gedrueckt
              Bit = 1 → Button losgelassen

Konvertierung: pressed = (raw & mask) == 0
```

### LED-Output (HC595, Active-High)

```
Hardware:     Bit = 1 → LED an
              Bit = 0 → LED aus

Konvertierung: Keine (One-Hot ist bereits Active-High)
```

### Datenfluss

```
CD4021 (Active-Low)     Firmware        HC595 (Active-High)
┌────────────────┐    ┌──────────┐    ┌────────────────┐
│ raw = 0x7F     │───►│ invert   │───►│ (nicht noetig) │
│ (Btn 1 pressed)│    │ & detect │    │                │
└────────────────┘    │ edges    │    └────────────────┘
                      └────┬─────┘
                           ▼
                      ┌──────────┐
                      │select_id │
                      │   = 1    │
                      └────┬─────┘
                           ▼
                      ┌──────────┐    ┌────────────────┐
                      │ one_hot  │───►│ out = 0x80     │
                      │          │    │ (LED 1 an)     │
                      └──────────┘    └────────────────┘
```

---

## 7) CRC-16 Algorithmus

### Zweck

Fehlererkennung im Serial-Protokoll (COBS-Frames).

### Parameter

| Parameter | Wert   |
|-----------|--------|
| Polynom   | 0x1021 |
| Init      | 0xFFFF |
| Name      | CRC-16 CCITT-FALSE |

### Algorithmus

```
FUNCTION crc16_ccitt(data[], len) -> uint16_t:
    crc = 0xFFFF

    FOR i = 0 TO len-1:
        crc ^= (data[i] << 8)

        FOR bit = 0 TO 7:
            IF (crc & 0x8000) != 0:
                crc = (crc << 1) ^ 0x1021
            ELSE:
                crc = crc << 1

    RETURN crc
```

### Beispiel

```
Input:  [0x01, 0x02, 0x03]
Output: 0x6131
```

---

## 8) COBS-Encoding

### Zweck

Byte-Stuffing fuer Frame-Delimiter (0x00) im Serial-Stream.

### Algorithmus (Encode)

```
FUNCTION cobs_encode(input[], in_len, output[]) -> size_t:
    read_idx = 0
    write_idx = 1  // Platz fuer erstes Overhead-Byte
    code_idx = 0
    code = 1

    WHILE read_idx < in_len:
        IF input[read_idx] == 0x00:
            output[code_idx] = code
            code_idx = write_idx
            write_idx++
            code = 1
        ELSE:
            output[write_idx] = input[read_idx]
            write_idx++
            code++

            IF code == 0xFF:
                output[code_idx] = code
                code_idx = write_idx
                write_idx++
                code = 1

        read_idx++

    output[code_idx] = code
    RETURN write_idx
```

### Beispiel

```
Input:  [0x01, 0x00, 0x02, 0x03]
Output: [0x02, 0x01, 0x03, 0x02, 0x03]
         │     │     │     └────────── Daten nach 0x00
         │     │     └──────────────── Naechstes 0x00 in 3 Bytes
         │     └────────────────────── Daten vor 0x00
         └──────────────────────────── Naechstes 0x00 in 2 Bytes
```

---

## Komplexitaets-Uebersicht

| Algorithmus    | Zeit  | Speicher | Anmerkung              |
|----------------|-------|----------|------------------------|
| Debounce       | O(n)  | O(n)     | n = Button-Anzahl      |
| Selection      | O(n)  | O(1)     | Early-Exit bei LOWEST  |
| Press-Guard    | O(n)  | O(n)     | Pro-Button Zustand     |
| Bit-Mapping    | O(1)  | O(1)     | Direkte Berechnung     |
| One-Hot        | O(n)  | O(1)     | n = Byte-Anzahl        |
| CRC-16         | O(n)  | O(1)     | n = Datenlaenge        |
| COBS           | O(n)  | O(n)     | n = Datenlaenge        |

---

## Siehe auch

- `FLOWCHART.md` – Visuelle Ablaufdiagramme
- `DEVELOPER.md` – API-Dokumentation
- `architecture.md` – Schichtenmodell
