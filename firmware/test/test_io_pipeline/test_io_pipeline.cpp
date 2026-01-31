/**
 * @file test_io_pipeline.cpp
 * @brief Unit Tests fuer I/O Pipeline.
 */

#include <cstdint>
#include <cstring>
#include <unity.h>

#include "hw/io_counts.h"
#include "logic/io_pipeline.h"

/* ============================================================================
 * HILFSFUNKTIONEN
 * ============================================================================ */

static void raw_all_released(uint8_t *raw) {
    std::memset(raw, 0xFF, HW::BTN_BYTES);
    raw[HW::BTN_BYTES - 1] &= HW::BTN_LAST_MASK;
}

// MSB-first, active-low: pressed => bit = 0
static void raw_set_pressed(uint8_t *raw, uint8_t id1, bool pressed) {
    const uint8_t byte = (uint8_t)((id1 - 1u) / 8u);
    const uint8_t mask = (uint8_t)(1u << (7u - ((id1 - 1u) % 8u)));

    if (pressed)
        raw[byte] = (uint8_t)(raw[byte] & ~mask);
    else
        raw[byte] = (uint8_t)(raw[byte] | mask);

    raw[HW::BTN_BYTES - 1] &= HW::BTN_LAST_MASK;
}

/* ============================================================================
 * TESTS
 * ============================================================================ */

void test_no_event_when_all_released() {
    io_pipeline::Pipeline p(io_pipeline::pipeline_cfg_t(
        30, 30, SELECTION_POLICY_LOWEST_ID_WINS, HW::BTN_COUNT));

    uint8_t raw[HW::BTN_BYTES];
    raw_all_released(raw);

    uint8_t sel = 0;
    TEST_ASSERT_FALSE(p.update(raw, sizeof(raw), 0, sel));
    TEST_ASSERT_EQUAL_UINT8(0, sel);
}

void test_debounce_single_press_edge_once() {
    io_pipeline::Pipeline p(io_pipeline::pipeline_cfg_t(
        30, 0, SELECTION_POLICY_LOWEST_ID_WINS, HW::BTN_COUNT));

    uint8_t raw[HW::BTN_BYTES];
    raw_all_released(raw);

    // press id=3
    raw_set_pressed(raw, 3, true);

    uint8_t sel = 0;

    // not stable yet
    TEST_ASSERT_FALSE(p.update(raw, sizeof(raw), 0, sel));
    TEST_ASSERT_FALSE(p.update(raw, sizeof(raw), 10, sel));

    // after debounce => event
    TEST_ASSERT_TRUE(p.update(raw, sizeof(raw), 40, sel));
    TEST_ASSERT_EQUAL_UINT8(3, sel);

    // held => no new events
    TEST_ASSERT_FALSE(p.update(raw, sizeof(raw), 100, sel));
}

void test_press_guard_blocks_fast_repress_same_button() {
    // debounce=0, guard=50ms
    io_pipeline::Pipeline p(io_pipeline::pipeline_cfg_t(
        0, 50, SELECTION_POLICY_LOWEST_ID_WINS, HW::BTN_COUNT));

    uint8_t raw[HW::BTN_BYTES];
    raw_all_released(raw);
    uint8_t sel = 0;

    // press 1 => select 1
    raw_set_pressed(raw, 1, true);
    TEST_ASSERT_TRUE(p.update(raw, sizeof(raw), 0, sel));
    TEST_ASSERT_EQUAL_UINT8(1, sel);

    // release 1
    raw_set_pressed(raw, 1, false);
    TEST_ASSERT_FALSE(p.update(raw, sizeof(raw), 1, sel));

    // press 2 => select 2 (different button)
    raw_set_pressed(raw, 2, true);
    TEST_ASSERT_TRUE(p.update(raw, sizeof(raw), 10, sel));
    TEST_ASSERT_EQUAL_UINT8(2, sel);

    // release 2
    raw_set_pressed(raw, 2, false);
    TEST_ASSERT_FALSE(p.update(raw, sizeof(raw), 11, sel));

    // re-press 1 within guard window => blocked
    raw_set_pressed(raw, 1, true);
    TEST_ASSERT_FALSE(p.update(raw, sizeof(raw), 30, sel));
    TEST_ASSERT_EQUAL_UINT8(0, sel);

    // release 1
    raw_set_pressed(raw, 1, false);
    TEST_ASSERT_FALSE(p.update(raw, sizeof(raw), 31, sel));

    // press 1 after guard window => allowed
    raw_set_pressed(raw, 1, true);
    TEST_ASSERT_TRUE(p.update(raw, sizeof(raw), 70, sel));
    TEST_ASSERT_EQUAL_UINT8(1, sel);
}

void test_policy_lowest_id_wins_same_time_edges() {
    io_pipeline::Pipeline p(io_pipeline::pipeline_cfg_t(
        0, 0, SELECTION_POLICY_LOWEST_ID_WINS, HW::BTN_COUNT));

    uint8_t raw[HW::BTN_BYTES];
    raw_all_released(raw);

    // both pressed before first update
    raw_set_pressed(raw, 2, true);
    raw_set_pressed(raw, 5, true);

    uint8_t sel = 0;
    TEST_ASSERT_TRUE(p.update(raw, sizeof(raw), 0, sel));
    TEST_ASSERT_EQUAL_UINT8(2, sel);
}

void test_policy_last_id_wins_same_time_edges() {
    io_pipeline::Pipeline p(io_pipeline::pipeline_cfg_t(
        0, 0, SELECTION_POLICY_LAST_ID_WINS, HW::BTN_COUNT));

    uint8_t raw[HW::BTN_BYTES];
    raw_all_released(raw);

    raw_set_pressed(raw, 2, true);
    raw_set_pressed(raw, 5, true);

    uint8_t sel = 0;
    TEST_ASSERT_TRUE(p.update(raw, sizeof(raw), 0, sel));
    TEST_ASSERT_EQUAL_UINT8(5, sel);
}

/* ============================================================================
 * ARDUINO / NATIVE ENTRY
 * ============================================================================ */

#ifdef ARDUINO
#include <Arduino.h>

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_no_event_when_all_released);
    RUN_TEST(test_debounce_single_press_edge_once);
    RUN_TEST(test_press_guard_blocks_fast_repress_same_button);
    RUN_TEST(test_policy_lowest_id_wins_same_time_edges);
    RUN_TEST(test_policy_last_id_wins_same_time_edges);
    UNITY_END();
}

void loop() {}
#else
int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_no_event_when_all_released);
    RUN_TEST(test_debounce_single_press_edge_once);
    RUN_TEST(test_press_guard_blocks_fast_repress_same_button);
    RUN_TEST(test_policy_lowest_id_wins_same_time_edges);
    RUN_TEST(test_policy_last_id_wins_same_time_edges);
    return UNITY_END();
}
#endif
