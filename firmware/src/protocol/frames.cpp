/**
 * @file frames.cpp
 * @brief Serial Protocol Frames Implementation.
 */

#include "frames.h"

#include <string.h>

namespace sp::proto {

/* ============================================================================
 * CRC-16 (CCITT-FALSE)
 * ============================================================================
 */

uint16_t crc16_ccitt_false(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t b = 0; b < 8; ++b) {
            if (crc & 0x8000)
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            else
                crc = (uint16_t)(crc << 1);
        }
    }
    return crc;
}

/* ============================================================================
 * PRIVATE HILFSFUNKTIONEN
 * ============================================================================
 */

static inline void write_u16_le(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}

static inline uint16_t read_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/* ============================================================================
 * COBS
 * ============================================================================
 */

size_t cobs_encode(const uint8_t *in, size_t in_len, uint8_t *out,
                   size_t out_cap) {
    if (!in || !out)
        return 0;
    if (out_cap == 0)
        return 0;

    size_t read_index = 0;
    size_t write_index = 1; // first code byte placeholder
    size_t code_index = 0;
    uint8_t code = 1;

    if (write_index > out_cap)
        return 0;

    while (read_index < in_len) {
        const uint8_t byte = in[read_index];

        if (byte == 0x00) {
            out[code_index] = code;
            code = 1;
            code_index = write_index;
            write_index++;
            if (write_index > out_cap)
                return 0;
            read_index++;
            continue;
        }

        // copy non-zero byte
        if (write_index >= out_cap)
            return 0;
        out[write_index++] = byte;
        read_index++;
        code++;

        // code byte full -> start new block
        if (code == 0xFF) {
            out[code_index] = code;
            code = 1;
            code_index = write_index;
            write_index++;
            if (write_index > out_cap)
                return 0;
        }
    }

    out[code_index] = code;
    return write_index;
}

bool cobs_decode(const uint8_t *in, size_t in_len, uint8_t *out, size_t out_cap,
                 size_t &out_len) {
    out_len = 0;
    if (!in || !out)
        return false;

    size_t read_index = 0;
    size_t write_index = 0;

    while (read_index < in_len) {
        const uint8_t code = in[read_index];
        if (code == 0)
            return false;

        if (read_index + (size_t)code > in_len + 1)
            return false;

        read_index++;

        for (uint8_t i = 1; i < code; ++i) {
            if (read_index >= in_len)
                return false;
            if (write_index >= out_cap)
                return false;
            out[write_index++] = in[read_index++];
        }

        // insert zero if this block was not length 0xFF and we're not at end
        if (code != 0xFF && read_index < in_len) {
            if (write_index >= out_cap)
                return false;
            out[write_index++] = 0x00;
        }
    }

    out_len = write_index;
    return true;
}

/* ============================================================================
 * PACKET BUILD/PARSE
 * ============================================================================
 */

bool build_packet(uint8_t *out, size_t out_cap, msg_type_e type, uint8_t flags,
                  uint16_t seq, const uint8_t *payload, uint16_t len,
                  size_t &out_len) {
    out_len = 0;
    if (!out)
        return false;
    if (len > MAX_PAYLOAD)
        return false;

    const size_t need = HEADER_LEN + (size_t)len + CRC_LEN;
    if (out_cap < need)
        return false;

    proto_header_t hdr{};
    hdr.magic0 = MAGIC0;
    hdr.magic1 = MAGIC1;
    hdr.ver_major = VER_MAJOR;
    hdr.ver_minor = VER_MINOR;
    hdr.type = (uint8_t)type;
    hdr.flags = flags;
    hdr.seq = seq;
    hdr.len = len;

    memcpy(out, &hdr, HEADER_LEN);

    if (len > 0) {
        if (!payload)
            return false;
        memcpy(out + HEADER_LEN, payload, len);
    }

    const uint16_t crc = crc16_ccitt_false(out, HEADER_LEN + len);
    write_u16_le(out + HEADER_LEN + len, crc);

    out_len = need;
    return true;
}

bool parse_packet(const uint8_t *in, size_t in_len, proto_header_t &hdr,
                  uint8_t *payload_out, size_t payload_cap) {
    if (!in)
        return false;
    if (in_len < HEADER_LEN + CRC_LEN)
        return false;

    memcpy(&hdr, in, HEADER_LEN);

    if (hdr.magic0 != MAGIC0 || hdr.magic1 != MAGIC1)
        return false;
    if (hdr.ver_major != VER_MAJOR)
        return false;

    const size_t len = (size_t)hdr.len;
    const size_t need = HEADER_LEN + len + CRC_LEN;
    if (in_len != need)
        return false;
    if (len > MAX_PAYLOAD)
        return false;
    if (len > payload_cap)
        return false;

    const uint16_t crc_rx = read_u16_le(in + HEADER_LEN + len);
    const uint16_t crc_ok = crc16_ccitt_false(in, HEADER_LEN + len);
    if (crc_rx != crc_ok)
        return false;

    if (len > 0) {
        if (!payload_out)
            return false;
        memcpy(payload_out, in + HEADER_LEN, len);
    }

    return true;
}

} // namespace sp::proto
