/**
 * @file test_selection.cpp
 * @brief Unit Tests fuer Selection-Logik.
 */

#include <Arduino.h>
#include <cstdint>
#include <cstring>
#include <unity.h>

#include "logic/selection.h"

/* ============================================================================
 * HILFSFUNKTIONEN
 * ============================================================================ */

static inline uint8_t mask_msb(uint8_t id1) {
    return (uint8_t)(1u << (7u - ((id1 - 1u) % 8u)));
}

static void edges_clear(uint8_t *edges, size_t n) { memset(edges, 0x00, n); }

static void edges_set(uint8_t *edges, uint8_t id1) {
    const uint8_t byte = (uint8_t)((id1 - 1u) / 8u);
    edges[byte] = (uint8_t)(edges[byte] | mask_msb(id1));
}

/* ============================================================================
 * TESTS
 * ============================================================================ */

void test_selection_no_edges_no_change() {
    selection_state_t st = {0};
    selection_cfg_t cfg;
    cfg.count = 10;
    cfg.policy = SELECTION_POLICY_LOWEST_ID_WINS;

    uint8_t edges[2];
    edges_clear(edges, sizeof(edges));

    uint8_t out = 0;
    TEST_ASSERT_FALSE(selection_update(&st, edges, sizeof(edges), &cfg, &out));
    TEST_ASSERT_EQUAL_UINT8(0, st.selected_id);
    TEST_ASSERT_EQUAL_UINT8(0, out);
}

void test_selection_lowest_id_wins() {
    selection_state_t st = {0};
    selection_cfg_t cfg;
    cfg.count = 10;
    cfg.policy = SELECTION_POLICY_LOWEST_ID_WINS;

    uint8_t edges[2];
    edges_clear(edges, sizeof(edges));
    edges_set(edges, 5);
    edges_set(edges, 2);

    uint8_t out = 0;
    TEST_ASSERT_TRUE(selection_update(&st, edges, sizeof(edges), &cfg, &out));
    TEST_ASSERT_EQUAL_UINT8(2, st.selected_id);
    TEST_ASSERT_EQUAL_UINT8(2, out);
}

void test_selection_last_id_wins() {
    selection_state_t st = {0};
    selection_cfg_t cfg;
    cfg.count = 10;
    cfg.policy = SELECTION_POLICY_LAST_ID_WINS;

    uint8_t edges[2];
    edges_clear(edges, sizeof(edges));
    edges_set(edges, 2);
    edges_set(edges, 5);

    uint8_t out = 0;
    TEST_ASSERT_TRUE(selection_update(&st, edges, sizeof(edges), &cfg, &out));
    TEST_ASSERT_EQUAL_UINT8(5, st.selected_id);
    TEST_ASSERT_EQUAL_UINT8(5, out);
}

void test_selection_ignores_invalid_bits() {
    selection_state_t st = {0};
    selection_cfg_t cfg;
    cfg.count = 10;
    cfg.policy = SELECTION_POLICY_LOWEST_ID_WINS;

    uint8_t edges[2];
    edges_clear(edges, sizeof(edges));

    // last byte valid bits are bits7..6 (IDs 9..10). Set only invalid bits.
    edges[1] = 0x3Fu;

    uint8_t out = 0;
    TEST_ASSERT_FALSE(selection_update(&st, edges, sizeof(edges), &cfg, &out));
    TEST_ASSERT_EQUAL_UINT8(0, st.selected_id);
    TEST_ASSERT_EQUAL_UINT8(0, out);
}

/* ============================================================================
 * ARDUINO ENTRY
 * ============================================================================ */

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_selection_no_edges_no_change);
    RUN_TEST(test_selection_lowest_id_wins);
    RUN_TEST(test_selection_last_id_wins);
    RUN_TEST(test_selection_ignores_invalid_bits);
    UNITY_END();
}

void loop() {}
