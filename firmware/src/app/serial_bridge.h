/**
 * @file serial_bridge.h
 * @brief Serial Bridge fuer Host-Kommunikation.
 *
 * Verarbeitet eingehende Befehle und sendet Statusmeldungen.
 */

#pragma once

#include "protocol/frames.h"
#include <Arduino.h>

namespace app {

/* ============================================================================
 * TYPES
 * ============================================================================ */

/**
 * @brief Empfangener CMD_SET_SELECTION Befehl.
 */
typedef struct cmd_set_selection_t {
    uint16_t selected_id; /**< 0..N (0 = alle aus) */
    uint8_t mode;         /**< 0 normal, 1 lock_local_until_next_cmd (optional) */
    bool ack_req;         /**< true wenn ACK erwartet */
    uint16_t rx_seq;      /**< Sequenznummer des empfangenen Befehls */
} cmd_set_selection_t;

/* ============================================================================
 * SERIAL BRIDGE KLASSE
 * ============================================================================ */

/**
 * @brief Serial Bridge Klasse.
 *
 * Handhabt COBS-geframte Kommunikation mit dem Host.
 */
class SerialBridge {
  public:
    /**
     * @brief Konstruktor.
     * @param s  Arduino Stream (z.B. Serial)
     */
    explicit SerialBridge(Stream &s)
        : _stream(s), _rx_frame{}, _rx_len(0), _raw{}, _enc{}, _tx_seq(1) {}

    /**
     * @brief Pollt fuer eingehende CMD_SET_SELECTION Befehle.
     *
     * @param out  Ausgabe: Empfangener Befehl
     * @return     true wenn ein Befehl empfangen wurde
     */
    bool poll_cmd_set_selection(cmd_set_selection_t &out);

    /**
     * @brief Sendet HELLO Message.
     *
     * @param btn_count    Anzahl Buttons
     * @param led_count    Anzahl LEDs
     * @param scan_ms      Scan-Periode in ms
     * @param debounce_ms  Entprellzeit in ms
     * @param feature_bits Feature-Flags
     */
    void send_hello(uint16_t btn_count, uint16_t led_count, uint16_t scan_ms,
                    uint16_t debounce_ms, uint8_t feature_bits);

    /**
     * @brief Sendet ACK fuer eine Sequenznummer.
     * @param seq_to_ack  Zu bestaetigende Sequenznummer
     */
    void send_ack(uint16_t seq_to_ack);

    /**
     * @brief Sendet SELECTION_STATE Message.
     *
     * @param selected_id  Ausgewaehlte ID (0..N)
     * @param source       Quelle (0=local_btn, 1=host_cmd, 2=reset, 3=recover)
     */
    void send_selection_state(uint16_t selected_id, uint8_t source);

  private:
    Stream &_stream;
    uint8_t _rx_frame[sp::proto::MAX_WIRE_FRAME];
    size_t _rx_len = 0;

    uint8_t _raw[sp::proto::MAX_PACKET];
    uint8_t _enc[sp::proto::MAX_WIRE_FRAME];

    uint16_t _tx_seq = 1;

    void _send_frame(sp::proto::msg_type_e type, uint8_t flags, uint16_t seq,
                     const uint8_t *payload, uint16_t len);
};

} // namespace app
