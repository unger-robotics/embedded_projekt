/**
 * @file test_e2e_hw.cpp
 * @brief E2E Hardware Tests (erfordert Hardware).
 */

#include <Arduino.h>
#include <cstdint>
#include <cstring>
#include <unity.h>

#include "hw/board_pins.h"
#include "hw/io_counts.h"
#include "hw/spi_params.h"

#include "drivers/cd4021.h"
#include "drivers/hc595.h"
#include "hal/spi_bus.h"

#include "logic/debounce.h"
#include "logic/selection.h"

/* ============================================================================
 * HARDWARE INSTANZEN
 * ============================================================================ */

static SpiBus bus;

static CD4021 buttons(bus, HW::PIN_BTN_PS, HW::PIN_BTN_MISO,
                      SPISettings(HW::SPI_4021_HZ, MSBFIRST,
                                  HW::SPI_4021_MODE));

static HC595 leds(bus, HW::PIN_LED_RCK, HW::PIN_LED_OE,
                  SPISettings(HW::SPI_595_HZ, MSBFIRST, HW::SPI_595_MODE));

static uint8_t btnRaw[HW::BTN_BYTES];
static uint8_t ledState[HW::LED_BYTES];

/* ============================================================================
 * HILFSFUNKTIONEN
 * ============================================================================ */

static inline uint8_t btn_mask_msb(uint8_t id1) {
    return (uint8_t)(1u << (7u - ((id1 - 1u) % 8u)));
}

static inline bool btn_pressed_msb(const uint8_t *raw, uint8_t id1) {
    const uint8_t byte = (uint8_t)((id1 - 1u) / 8u);
    return (raw[byte] & btn_mask_msb(id1)) == 0u; // active-low
}

static inline uint8_t btn_last_mask_msb(uint8_t count) {
    const uint8_t rem = (uint8_t)(count % 8u);
    if (rem == 0)
        return 0xFFu;
    return (uint8_t)(0xFFu << (8u - rem));
}

static void read_buttons_mask_invalid() {
    buttons.read(btnRaw, sizeof(btnRaw));
    const uint8_t m = btn_last_mask_msb(HW::BTN_COUNT);
    btnRaw[HW::BTN_BYTES - 1] |= (uint8_t)(~m);
}

static bool wait_for_button_state(uint8_t id1, bool pressed,
                                  uint32_t timeout_ms,
                                  uint8_t stable_samples = 6,
                                  uint32_t sample_delay_ms = 5) {
    const uint32_t start = millis();
    uint8_t ok = 0;

    while ((millis() - start) < timeout_ms) {
        read_buttons_mask_invalid();
        const bool p = btn_pressed_msb(btnRaw, id1);

        if (p == pressed) {
            if (++ok >= stable_samples)
                return true;
        } else {
            ok = 0;
        }
        delay(sample_delay_ms);
    }
    return false;
}

static bool all_released_now() {
    for (uint8_t id = 1; id <= HW::BTN_COUNT; ++id) {
        if (btn_pressed_msb(btnRaw, id))
            return false;
    }
    return true;
}

static bool wait_for_all_released(uint32_t timeout_ms) {
    const uint32_t start = millis();
    uint8_t ok = 0;

    while ((millis() - start) < timeout_ms) {
        read_buttons_mask_invalid();
        if (all_released_now()) {
            if (++ok >= 6)
                return true;
        } else {
            ok = 0;
        }
        delay(5);
    }
    return false;
}

static void setOneHot(uint8_t selected_id) {
    memset(ledState, 0x00, sizeof(ledState));
    if (selected_id >= 1 && selected_id <= HW::LED_COUNT) {
        const uint8_t bit = (uint8_t)(selected_id - 1);
        ledState[bit / 8] |= (uint8_t)(1u << (bit % 8));
    }
    ledState[HW::LED_BYTES - 1] &= HW::LED_LAST_MASK;
}

/* ============================================================================
 * TESTS
 * ============================================================================ */

static void test_pins_sanity() {
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, HW::PIN_SCK, "PIN_SCK invalid");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, HW::PIN_BTN_MISO, "PIN_BTN_MISO invalid");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, HW::PIN_LED_MOSI, "PIN_LED_MOSI invalid");

    TEST_ASSERT_NOT_EQUAL_MESSAGE(HW::PIN_SCK, HW::PIN_BTN_MISO,
                                  "SCK == MISO (wiring?)");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(HW::PIN_SCK, HW::PIN_LED_MOSI,
                                  "SCK == MOSI (wiring?)");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(HW::PIN_BTN_MISO, HW::PIN_LED_MOSI,
                                  "MISO == MOSI (wiring?)");

    Serial.printf("Pins: SCK=%d MISO=%d MOSI=%d PS=%d RCK=%d OE=%d\n",
                  HW::PIN_SCK, HW::PIN_BTN_MISO, HW::PIN_LED_MOSI,
                  HW::PIN_BTN_PS, HW::PIN_LED_RCK, HW::PIN_LED_OE);
}

static void test_cd4021_idle_all_released() {
    Serial.println("[E2E] Bitte KEINEN Taster druecken (2s)...");
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_all_released(2000),
        "Nicht alle Inputs sind stabil released (Pullups/Wiring pruefen)");

    read_buttons_mask_invalid();
    Serial.printf("[E2E] CD4021 RAW:");
    for (size_t i = 0; i < sizeof(btnRaw); ++i)
        Serial.printf(" 0x%02X", btnRaw[i]);
    Serial.println();
}

static void test_e2e_button1_debounce_selection_and_onehot() {
    Serial.println("[E2E] Taster 1 druecken & halten (Timeout 8s)...");
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_button_state(1, true, 8000),
        "Taster 1 nicht erkannt (Mapping/PS/CLK/MISO pruefen)");

    // Debounce+Selection gegen echte Raw-Bytes
    debounce_cfg_t dcfg;
    dcfg.debounce_ms = 30;
    dcfg.count = HW::BTN_COUNT;
    Debouncer deb(dcfg);

    selection_state_t st = {0};
    selection_cfg_t scfg;
    scfg.count = HW::BTN_COUNT;
    scfg.policy = SELECTION_POLICY_LOWEST_ID_WINS;

    uint8_t edges[HW::BTN_BYTES] = {0};
    uint8_t outSel = 0;

    const uint32_t start = millis();
    bool got = false;

    while ((millis() - start) < 2000) {
        read_buttons_mask_invalid();
        deb.update(btnRaw, sizeof(btnRaw), millis());
        deb.getPressedEdges(edges, sizeof(edges));

        if (selection_update(&st, edges, sizeof(edges), &scfg, &outSel)) {
            if (outSel == 1) {
                got = true;
                break;
            }
        }
        delay(2);
    }

    TEST_ASSERT_TRUE_MESSAGE(
        got,
        "Debounce/Selection hat Button 1 nicht als Selection-Event geliefert");

    setOneHot(1);
    leds.write(ledState, sizeof(ledState));

    uint8_t expected[HW::LED_BYTES];
    memset(expected, 0x00, sizeof(expected));
    expected[0] = 0x01; // LED1 = bit0
    if (HW::LED_BYTES > 1)
        expected[1] = 0x00;
    expected[HW::LED_BYTES - 1] &= HW::LED_LAST_MASK;

    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, ledState, HW::LED_BYTES);

    Serial.println("[E2E] OK: Button1->Selection(1)->LED one-hot gesetzt. "
                   "Jetzt Taster 1 loslassen...");
    TEST_ASSERT_TRUE_MESSAGE(wait_for_button_state(1, false, 5000),
                             "Taster 1 Release nicht erkannt");
}

static void test_e2e_button10_mapping() {
    Serial.println("[E2E] Taster 10 druecken & halten (Timeout 8s)...");
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_button_state(10, true, 8000),
        "Taster 10 nicht erkannt (Mapping/PS/CLK/MISO pruefen)");

    read_buttons_mask_invalid();
    TEST_ASSERT_TRUE_MESSAGE(
        btn_pressed_msb(btnRaw, 10),
        "Raw-Mapping fuer Taster 10 passt nicht (Bitposition falsch?)");

    Serial.println(
        "[E2E] OK: Raw-Mapping fuer Button 10 passt. Jetzt loslassen...");
    TEST_ASSERT_TRUE_MESSAGE(wait_for_button_state(10, false, 5000),
                             "Taster 10 Release nicht erkannt");
}

static void test_hc595_walking_one_visual_confirm() {
    Serial.println("[E2E] Walking-One auf LEDs (ca. 2s). Danach: wenn OK, "
                   "Taster 1 druecken zur Bestaetigung.");

    for (uint8_t id = 1; id <= HW::LED_COUNT; ++id) {
        setOneHot(id);
        leds.write(ledState, sizeof(ledState));
        delay(150);
    }
    setOneHot(0);
    leds.write(ledState, sizeof(ledState));

    Serial.println("[E2E] Wenn LED-Sequenz korrekt war: jetzt Taster 1 druecken "
                   "(Timeout 8s)...");
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_button_state(1, true, 8000),
        "Bestaetigung nicht erhalten (oder Button1 Mapping falsch)");
    TEST_ASSERT_TRUE_MESSAGE(wait_for_button_state(1, false, 5000),
                             "Button1 Release nicht erkannt");
}

/* ============================================================================
 * ARDUINO ENTRY
 * ============================================================================ */

void setup() {
    delay(1500);
    Serial.begin(115200);
    delay(200);

    bus.begin(HW::PIN_SCK, HW::PIN_BTN_MISO, HW::PIN_LED_MOSI);
    buttons.begin();
    leds.begin(true);

    setOneHot(0);
    leds.write(ledState, sizeof(ledState));

    UNITY_BEGIN();
    RUN_TEST(test_pins_sanity);
    RUN_TEST(test_cd4021_idle_all_released);
    RUN_TEST(test_e2e_button1_debounce_selection_and_onehot);
    RUN_TEST(test_e2e_button10_mapping);
    RUN_TEST(test_hc595_walking_one_visual_confirm);
    UNITY_END();
}

void loop() {}
