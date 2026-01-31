/**
 * @file frames.h
 * @brief Serial Protocol Frames (COBS + CRC-16).
 *
 * Wire framing:
 *   - Raw packet: Header || Payload || CRC16
 *   - On-wire: COBS(raw_packet) + 0x00 delimiter
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

namespace sp::proto {

/* ============================================================================
 * KONSTANTEN
 * ============================================================================ */

static constexpr uint8_t MAGIC0 = 'S';
static constexpr uint8_t MAGIC1 = 'P';
static constexpr uint8_t VER_MAJOR = 1;
static constexpr uint8_t VER_MINOR = 0;

static constexpr uint8_t FLAG_ACK_REQ = 0x01;
static constexpr uint8_t FLAG_IS_ACK = 0x02;
static constexpr uint8_t FLAG_IS_ERR = 0x04;

static constexpr uint8_t FRAME_DELIM = 0x00; /**< Delimiter nach COBS Frame */

/* ============================================================================
 * TYPES
 * ============================================================================ */

/**
 * @brief Message-Typ Enumeration.
 */
typedef enum msg_type_e : uint8_t {
    MSG_TYPE_HELLO = 0x01,
    MSG_TYPE_HELLO_ACK = 0x02,
    MSG_TYPE_PING = 0x03,
    MSG_TYPE_PONG = 0x04,

    MSG_TYPE_BTN_EVENT = 0x10,
    MSG_TYPE_SNAPSHOT = 0x11,
    MSG_TYPE_SELECTION_STATE = 0x12,

    MSG_TYPE_CMD_SET_SELECTION = 0x20,
    MSG_TYPE_CMD_REQ_SNAPSHOT = 0x23,

    MSG_TYPE_ACK = 0x30,
    MSG_TYPE_ERR = 0x50,
} msg_type_e;

#pragma pack(push, 1)
/**
 * @brief Protokoll-Header (Little-Endian fuer Multi-Byte Felder).
 */
typedef struct proto_header_t {
    uint8_t magic0;    /**< 'S' */
    uint8_t magic1;    /**< 'P' */
    uint8_t ver_major; /**< Version Major (1) */
    uint8_t ver_minor; /**< Version Minor (0) */
    uint8_t type;      /**< msg_type_e */
    uint8_t flags;     /**< FLAG_* */
    uint16_t seq;      /**< Sequenznummer */
    uint16_t len;      /**< Payload-Laenge in Bytes */
} proto_header_t;
#pragma pack(pop)

/* ============================================================================
 * GROESSEN-KONSTANTEN
 * ============================================================================ */

static constexpr size_t HEADER_LEN = sizeof(proto_header_t);
static constexpr size_t CRC_LEN = 2;

/** Maximale Payload-Groesse (klein und deterministisch) */
static constexpr size_t MAX_PAYLOAD = 256;

/** Raw Packet Groesse (vor COBS) */
static constexpr size_t MAX_PACKET = HEADER_LEN + MAX_PAYLOAD + CRC_LEN;

/** Worst-case COBS Overhead: + (n/254) + 1 Bytes */
static constexpr size_t MAX_COBS_OVERHEAD = (MAX_PACKET / 254) + 1;

/** Maximale Wire-Frame Groesse (COBS + Delimiter) */
static constexpr size_t MAX_WIRE_FRAME = MAX_PACKET + MAX_COBS_OVERHEAD + 1;

/* ============================================================================
 * OEFFENTLICHE FUNKTIONEN
 * ============================================================================ */

/**
 * @brief CRC-16/CCITT-FALSE Berechnung.
 *
 * poly: 0x1021, init: 0xFFFF, xorout: 0x0000, refin/refout: false
 *
 * @param data  Daten-Buffer
 * @param len   Laenge
 * @return      CRC-16 Wert
 */
uint16_t crc16_ccitt_false(const uint8_t *data, size_t len);

/**
 * @brief COBS Encode.
 *
 * @param in      Eingabe-Buffer
 * @param in_len  Eingabe-Laenge
 * @param out     Ausgabe-Buffer
 * @param out_cap Ausgabe-Kapazitaet
 * @return        Kodierte Laenge, oder 0 bei Fehler
 */
size_t cobs_encode(const uint8_t *in, size_t in_len, uint8_t *out,
                   size_t out_cap);

/**
 * @brief COBS Decode.
 *
 * @param in      Eingabe-Buffer
 * @param in_len  Eingabe-Laenge
 * @param out     Ausgabe-Buffer
 * @param out_cap Ausgabe-Kapazitaet
 * @param out_len Ausgabe: Dekodierte Laenge
 * @return        true bei Erfolg
 */
bool cobs_decode(const uint8_t *in, size_t in_len, uint8_t *out, size_t out_cap,
                 size_t &out_len);

/**
 * @brief Baut ein Raw Packet: Header||Payload||CRC.
 *
 * @param out     Ausgabe-Buffer
 * @param out_cap Ausgabe-Kapazitaet
 * @param type    Message-Typ
 * @param flags   Flags
 * @param seq     Sequenznummer
 * @param payload Payload-Buffer (kann NULL sein)
 * @param len     Payload-Laenge
 * @param out_len Ausgabe: Gesamte Paket-Laenge
 * @return        true bei Erfolg
 */
bool build_packet(uint8_t *out, size_t out_cap, msg_type_e type, uint8_t flags,
                  uint16_t seq, const uint8_t *payload, uint16_t len,
                  size_t &out_len);

/**
 * @brief Parst ein Raw Packet und verifiziert Magic/Version/CRC.
 *
 * @param in          Eingabe-Buffer (bereits COBS-dekodiert)
 * @param in_len      Eingabe-Laenge
 * @param hdr         Ausgabe: Header
 * @param payload_out Ausgabe: Payload
 * @param payload_cap Payload-Buffer Kapazitaet
 * @return            true bei Erfolg
 */
bool parse_packet(const uint8_t *in, size_t in_len, proto_header_t &hdr,
                  uint8_t *payload_out, size_t payload_cap);

} // namespace sp::proto

#ifdef __cplusplus
}
#endif
