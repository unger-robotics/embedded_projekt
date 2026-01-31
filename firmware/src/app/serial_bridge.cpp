/**
 * @file serial_bridge.cpp
 * @brief Serial Bridge Implementation.
 */

#include "app/serial_bridge.h"
#include <string.h>

namespace app {

/* ============================================================================
 * PRIVATE METHODEN
 * ============================================================================
 */

void SerialBridge::_send_frame(sp::proto::msg_type_e type, uint8_t flags,
                               uint16_t seq, const uint8_t *payload,
                               uint16_t len) {
    size_t raw_len = 0;
    if (!sp::proto::build_packet(_raw, sizeof(_raw), type, flags, seq, payload,
                                 len, raw_len))
        return;

    const size_t enc_len =
        sp::proto::cobs_encode(_raw, raw_len, _enc, sizeof(_enc));
    if (enc_len == 0)
        return;

    _stream.write(_enc, enc_len);
    _stream.write(sp::proto::FRAME_DELIM);
}

/* ============================================================================
 * OEFFENTLICHE METHODEN
 * ============================================================================
 */

bool SerialBridge::poll_cmd_set_selection(cmd_set_selection_t &out) {
    while (_stream.available() > 0) {
        const uint8_t b = (uint8_t)_stream.read();

        if (b == sp::proto::FRAME_DELIM) {
            if (_rx_len == 0)
                continue;

            size_t decoded_len = 0;
            if (!sp::proto::cobs_decode(_rx_frame, _rx_len, _raw, sizeof(_raw),
                                        decoded_len)) {
                _rx_len = 0;
                continue;
            }

            sp::proto::proto_header_t hdr{};
            uint8_t payload[sp::proto::MAX_PAYLOAD]{};
            if (!sp::proto::parse_packet(_raw, decoded_len, hdr, payload,
                                         sizeof(payload))) {
                _rx_len = 0;
                continue;
            }

            _rx_len = 0;

            const auto type = (sp::proto::msg_type_e)hdr.type;
            if (type == sp::proto::MSG_TYPE_CMD_SET_SELECTION && hdr.len == 4) {
                const uint16_t selected =
                    (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
                const uint8_t mode = payload[2];
                out.selected_id = selected;
                out.mode = mode;
                out.ack_req = (hdr.flags & sp::proto::FLAG_ACK_REQ) != 0;
                out.rx_seq = hdr.seq;
                return true;
            }

            continue;
        }

        if (_rx_len < sizeof(_rx_frame))
            _rx_frame[_rx_len++] = b;
        else
            _rx_len = 0; // overflow -> drop
    }

    return false;
}

void SerialBridge::send_hello(uint16_t btn_count, uint16_t led_count,
                              uint16_t scan_ms, uint16_t debounce_ms,
                              uint8_t feature_bits) {
    uint8_t p[10]{};
    p[0] = btn_count & 0xFF;
    p[1] = btn_count >> 8;
    p[2] = led_count & 0xFF;
    p[3] = led_count >> 8;
    p[4] = scan_ms & 0xFF;
    p[5] = scan_ms >> 8;
    p[6] = debounce_ms & 0xFF;
    p[7] = debounce_ms >> 8;
    p[8] = feature_bits;
    p[9] = 0;

    _send_frame(sp::proto::MSG_TYPE_HELLO, 0, _tx_seq++, p, sizeof(p));
}

void SerialBridge::send_ack(uint16_t seq_to_ack) {
    _send_frame(sp::proto::MSG_TYPE_ACK, sp::proto::FLAG_IS_ACK, seq_to_ack,
                nullptr, 0);
}

void SerialBridge::send_selection_state(uint16_t selected_id, uint8_t source) {
    uint8_t p[8]{};
    p[0] = selected_id & 0xFF;
    p[1] = selected_id >> 8;
    p[2] = source;
    p[3] = 0;
    const uint32_t t = millis();
    p[4] = (uint8_t)(t & 0xFF);
    p[5] = (uint8_t)((t >> 8) & 0xFF);
    p[6] = (uint8_t)((t >> 16) & 0xFF);
    p[7] = (uint8_t)((t >> 24) & 0xFF);

    _send_frame(sp::proto::MSG_TYPE_SELECTION_STATE, 0, _tx_seq++, p,
                sizeof(p));
}

} // namespace app
