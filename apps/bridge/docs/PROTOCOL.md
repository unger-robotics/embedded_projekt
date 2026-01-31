# Protocol (SSOT)

Browser-Dashboard ↔ WebSocket(JSON) ↔ Bridge(Python) ↔ Serial(USB) ↔ ESP32

**Gültigkeit:** Bridge `apps/bridge/*` + Firmware-Protokoll `firmware/src/protocol/*` (falls abweichend: **dieses Dokument ist SSOT**).

---

## 1) Serial Wire Format

### 1.1 Framing

- Transport: UART/USB-Serial (Byte-Stream)
- Frame-Encoding: **COBS**
- Frame-Delimiter: `0x00` (1 Byte)
- Max Frame Size (Bridge, **ohne** Delimiter): `2048` Bytes

**Wire:**

```
COBS(packet_bytes) + 0x00
```

**Hinweis:** Wenn die Bridge ein Frame > Max empfängt, verwirft sie das Frame und resynchronisiert am nächsten `0x00`.

### 1.2 Packet Layout (Little Endian)

Packet = Header (10 Bytes) + Payload (`payload_len`) + CRC16 (2 Bytes)

| Offset | Typ   | Name                                |
|-------:|-------|-------------------------------------|
|      0 | u8    | `MAGIC0` = `'S'`                    |
|      1 | u8    | `MAGIC1` = `'P'`                    |
|      2 | u8    | `VER_MAJOR` (= `1`)                 |
|      3 | u8    | `VER_MINOR` (= `0`, reserviert)     |
|      4 | u8    | `msg_type`                          |
|      5 | u8    | `flags`                             |
|      6 | u16   | `seq`                               |
|      8 | u16   | `payload_len`                       |
|     10 | bytes | `payload`                           |
| 10+len | u16   | `crc16_ccitt_false(header+payload)` |

**CRC:** `CRC16/CCITT-FALSE` (`init=0xFFFF`, `poly=0x1021`, `xorout=0x0000`, no reflect).

### 1.3 Flags

- `0x01` = `ACK_REQ` (Sender erwartet ACK)
- `0x02` = `IS_ACK` (reserviert; aktuell nicht erforderlich)

### 1.4 Message Types

> Alle Payload-Felder sind **Little Endian**.

#### `TYPE_HELLO = 0x01` (ESP32 → Bridge)

**Payload:** mindestens 10 Bytes erwartet; genutzt werden die ersten 10 Bytes.

| Offset | Typ | Name             |
|-------:|-----|------------------|
|      0 | u16 | `btn_count`      |
|      2 | u16 | `led_count`      |
|      4 | u16 | `scan_period_ms` |
|      6 | u16 | `debounce_ms`    |
|      8 | u8  | `feature_bits`   |
|      9 | u8  | `reserved`       |

**Semantik:**
- `btn_count`, `led_count` > 0
- `feature_bits`: projektspezifisches Bitfeld

#### `TYPE_SELECTION_STATE = 0x12` (ESP32 → Bridge)

**Payload:** genau 8 Bytes.

| Offset | Typ | Name          |
|-------:|-----|---------------|
|      0 | u16 | `selected_id` |
|      2 | u8  | `source`      |
|      3 | u8  | `reserved`    |
|      4 | u32 | `t_ms`        |

**Semantik:**
- `selected_id`: `0..btn_count` (`0` = alle aus)
- `source`:
  - `0` = `local_btn`
  - `1` = `host_cmd`
  - `2` = `reset`
  - `3` = `recover`
- `t_ms`: Uptime in ms (ESP32 Zeitbasis)

#### `TYPE_CMD_SET_SELECTION = 0x20` (Bridge → ESP32)

**Payload:** 4 Bytes (`<HBB>`)

| Offset | Typ | Name          |
|-------:|-----|---------------|
|      0 | u16 | `selected_id` |
|      2 | u8  | `reserved0`   |
|      3 | u8  | `reserved1`   |

**Semantik:**
- `selected_id`: `0..btn_count` (`0` = alle aus, sonst one-hot)
- Bridge setzt i. d. R. `ACK_REQ`.

#### `TYPE_ACK = 0x30` (ESP32 → Bridge)

**Payload:** optional/leer (Bridge wertet primär `seq` aus)

**Semantik:**
- Bestätigt `seq` eines zuvor empfangenen Commands (Bridge → ESP32).

---

## 2) WebSocket JSON (Browser ↔ Bridge)

**Endpoint:**
- `ws://<host>:<port>/ws`

### 2.1 Commands (Browser → Bridge)

#### `setSelection`

```json
{ "type": "setSelection", "selected": 8 }
```

* `selected`: `0..btnCount` (`0` = alle aus)

### 2.2 Events (Bridge → Browser)

#### `hello`

```json
{
  "type": "hello",
  "btnCount": 10,
  "ledCount": 10,
  "scanPeriodMs": 1,
  "debounceMs": 30,
  "features": 0
}
```

#### `selection`

```json
{
  "type": "selection",
  "selected": 8,
  "source": "host_cmd",
  "tMs": 123456
}
```

* `source`: `"local_btn" | "host_cmd" | "reset" | "recover"`

#### `serial`

```json
{ "type": "serial", "state": "connected", "port": "/dev/ttyACM0" }
```

* `state`: `connected | disconnected | reconnecting | unknown` (Bridge kann erweitert werden)

#### `ack`

```json
{ "type": "ack", "seq": 4 }
```

#### `warn`

```json
{ "type": "warn", "code": 20, "detail": "ack_timeout ..." }
```

#### `error`

```json
{ "type": "error", "code": 30, "detail": "serial_not_connected" }
```

### 2.3 Typische Codes (Bridge)

* `2`  `invalid_json`
* `11` `frame_too_long` (resync)
* `12` `frame_over_max`
* `20` `ack_timeout`
* `21` `ack_failed`
* `22` `serial_send_failed`
* `30` `serial_not_connected`
* `40` `serial_reconnect` (info/warn)

---

## 3) Invarianten

* **One-hot:** `selected_id = 0` oder genau eine Auswahl `1..btn_count`.
* **ACK-Flow:** Bridge sendet Commands mit `ACK_REQ`; ESP32 antwortet mit `TYPE_ACK` (gleicher `seq`).
* **Recover:** Nach WS-Connect sendet die Bridge zeitnah einen `selection`-Event (aktueller Zustand), sofern bekannt.


