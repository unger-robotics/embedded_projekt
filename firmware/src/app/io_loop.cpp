/**
 * @file io_loop.cpp
 * @brief Arduino-Loop I/O (One-Hot + Serial CMD_SET_SELECTION).
 *
 * Single-threaded Polling ohne FreeRTOS.
 */

#include <Arduino.h>
#include <SPI.h>
#include <string.h>

#include "hw/board_pins.h"
#include "hw/io_counts.h"
#include "hw/spi_params.h"

#include "drivers/cd4021.h"
#include "drivers/hc595.h"
#include "hal/spi_bus.h"

#include "logic/io_pipeline.h"
#include "protocol/frames.h"

#include "app/serial_bridge.h"

namespace app_loop {

/* ============================================================================
 * HW
 * ============================================================================
 */

static SpiBus bus;

static CD4021 buttons(bus, HW::PIN_BTN_PS, HW::PIN_BTN_MISO,
                      SPISettings(HW::SPI_4021_HZ, MSBFIRST,
                                  HW::SPI_4021_MODE));

static HC595 leds(bus, HW::PIN_LED_RCK, HW::PIN_LED_OE,
                  SPISettings(HW::SPI_595_HZ, MSBFIRST, HW::SPI_595_MODE));

static uint8_t btnRaw[HW::BTN_BYTES];
static uint8_t ledState[HW::LED_BYTES];

/**
 * @brief Setzt LED-Zustand fuer One-Hot Auswahl.
 */
static inline void setOneHot(uint8_t selected_id) {
    memset(ledState, 0x00, sizeof(ledState));
    if (selected_id >= 1 && selected_id <= HW::LED_COUNT) {
        const uint8_t bit = (uint8_t)(selected_id - 1u);
        ledState[bit / 8u] |= (uint8_t)(1u << (bit % 8u));
    }
    ledState[HW::LED_BYTES - 1] &= HW::LED_LAST_MASK;
}

/* ============================================================================
 * LOGIC
 * ============================================================================
 */

static io_pipeline::Pipeline pipeline(io_pipeline::pipeline_cfg_t(
    30, 30, SELECTION_POLICY_LOWEST_ID_WINS, HW::BTN_COUNT));

/* ============================================================================
 * SERIAL PROTOCOL (Local Helpers)
 * ============================================================================
 */

static uint8_t rx_frame[sp::proto::MAX_WIRE_FRAME];
static size_t rx_len = 0;

static uint8_t raw_buf[sp::proto::MAX_PACKET];
static uint8_t cobs_buf[sp::proto::MAX_WIRE_FRAME];
static uint16_t tx_seq = 1;

static inline void serial_send_frame(sp::proto::msg_type_e type, uint8_t flags,
                                     uint16_t seq, const uint8_t *payload,
                                     uint16_t len) {
    size_t raw_len = 0;
    if (!sp::proto::build_packet(raw_buf, sizeof(raw_buf), type, flags, seq,
                                 payload, len, raw_len))
        return;

    const size_t enc_len =
        sp::proto::cobs_encode(raw_buf, raw_len, cobs_buf, sizeof(cobs_buf));
    if (enc_len == 0)
        return;

    Serial.write(cobs_buf, enc_len);
    Serial.write(sp::proto::FRAME_DELIM);
}

static inline void serial_send_ack(uint16_t seq_to_ack) {
    serial_send_frame(sp::proto::MSG_TYPE_ACK, sp::proto::FLAG_IS_ACK,
                      seq_to_ack, nullptr, 0);
}

static inline void serial_send_hello() {
    uint8_t p[10]{};

    const uint16_t btn_count = HW::BTN_COUNT;
    const uint16_t led_count = HW::LED_COUNT;
    const uint16_t scan_ms = 1;
    const uint16_t debounce_ms = 30;

    p[0] = btn_count & 0xFF;
    p[1] = btn_count >> 8;
    p[2] = led_count & 0xFF;
    p[3] = led_count >> 8;
    p[4] = scan_ms & 0xFF;
    p[5] = scan_ms >> 8;
    p[6] = debounce_ms & 0xFF;
    p[7] = debounce_ms >> 8;

    uint8_t feat = 0;
    feat |= (1u << 0); // onehot
    feat |= (1u << 3); // selection_state supported
    p[8] = feat;
    p[9] = 0;

    serial_send_frame(sp::proto::MSG_TYPE_HELLO, 0, tx_seq++, p, sizeof(p));
}

static inline void serial_send_selection_state(uint16_t selected_id,
                                               uint8_t source) {
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

    serial_send_frame(sp::proto::MSG_TYPE_SELECTION_STATE, 0, tx_seq++, p,
                      sizeof(p));
}

/**
 * @brief Pollt fuer CMD_SET_SELECTION.
 */
static bool serial_poll_cmd_set_selection(app::cmd_set_selection_t &out) {
    while (Serial.available() > 0) {
        const uint8_t b = (uint8_t)Serial.read();

        if (b == sp::proto::FRAME_DELIM) {
            if (rx_len == 0)
                continue;

            size_t decoded_len = 0;
            if (!sp::proto::cobs_decode(rx_frame, rx_len, raw_buf,
                                        sizeof(raw_buf), decoded_len)) {
                rx_len = 0;
                continue;
            }

            sp::proto::proto_header_t hdr{};
            uint8_t payload[sp::proto::MAX_PAYLOAD]{};
            if (!sp::proto::parse_packet(raw_buf, decoded_len, hdr, payload,
                                         sizeof(payload))) {
                rx_len = 0;
                continue;
            }

            rx_len = 0;

            const auto type = (sp::proto::msg_type_e)hdr.type;
            if (type == sp::proto::MSG_TYPE_CMD_SET_SELECTION && hdr.len == 4) {
                out.selected_id =
                    (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
                out.mode = payload[2];
                out.ack_req = (hdr.flags & sp::proto::FLAG_ACK_REQ) != 0;
                out.rx_seq = hdr.seq;
                return true;
            }

            continue;
        }

        if (rx_len < sizeof(rx_frame))
            rx_frame[rx_len++] = b;
        else
            rx_len = 0; // overflow -> drop
    }

    return false;
}

/* ============================================================================
 * ARDUINO
 * ============================================================================
 */

void setup() {
    Serial.begin(921600);

    const uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 1000) {
        delay(10);
    }

    bus.begin(HW::PIN_SCK, HW::PIN_BTN_MISO, HW::PIN_LED_MOSI);
    buttons.begin();
    leds.begin(true);

    setOneHot(0);
    leds.write(ledState, sizeof(ledState));

    serial_send_hello();
}

void loop() {
    static uint32_t lastHelloMs = 0;
    static uint32_t lastTickMs = 0;

    // 1) RX: CMD_SET_SELECTION (ACK sofort, apply spaeter)
    app::cmd_set_selection_t lastCmd{};
    bool haveCmd = false;

    app::cmd_set_selection_t cmd{};
    while (serial_poll_cmd_set_selection(cmd)) {
        if (cmd.ack_req) {
            serial_send_ack(cmd.rx_seq);
        }
        lastCmd = cmd;
        haveCmd = true;
    }

    // 2) HELLO zyklisch
    const uint32_t nowMs = millis();
    if (nowMs - lastHelloMs > 2000u) {
        serial_send_hello();
        lastHelloMs = nowMs;
    }

    // 3) 1 ms Tick
    if ((uint32_t)(nowMs - lastTickMs) < 1u) {
        delay(0); // yield
        return;
    }
    lastTickMs = nowMs;

    // 4) Host cmd anwenden (preempt)
    if (haveCmd) {
        uint16_t sel = lastCmd.selected_id;
        if (sel > HW::BTN_COUNT)
            sel = 0;

        pipeline.force_selected((uint8_t)sel);

        setOneHot((uint8_t)sel);
        leds.write(ledState, sizeof(ledState));

        serial_send_selection_state(sel, 1 /*host_cmd*/);
        return;
    }

    // 5) Local scan + pipeline
    buttons.read(btnRaw, sizeof(btnRaw));

    uint8_t newSelected = 0;
    if (pipeline.update(btnRaw, sizeof(btnRaw), nowMs, newSelected)) {
        setOneHot(newSelected);
        leds.write(ledState, sizeof(ledState));
        serial_send_selection_state(newSelected, 0 /*local_btn*/);
    }
}

} // namespace app_loop
