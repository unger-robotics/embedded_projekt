/**
 * @file test_debounce.cpp
 * @brief Unit Tests fuer Debounce-Logik.
 */

#include <Arduino.h>
#include <cstdint>
#include <cstring>
#include <unity.h>

#include "logic/debounce.h"

/* ============================================================================
 * HILFSFUNKTIONEN
 * ============================================================================ */

static inline uint8_t mask_msb(uint8_t id1) {
    return (uint8_t)(1u << (7u - ((id1 - 1u) % 8u)));
}

static void raw_all_released(uint8_t *raw, uint8_t bytes, uint8_t count) {
    memset(raw, 0xFF, bytes);
    // mask invalid bits in last byte to released=1 (MSB-first)
    const uint8_t rem = (uint8_t)(count % 8u);
    const uint8_t lastMask =
        (rem == 0) ? 0xFFu : (uint8_t)(0xFFu << (8u - rem));
    raw[bytes - 1] = (uint8_t)(raw[bytes - 1] | (uint8_t)(~lastMask));
}

// MSB-first, active-low: pressed => bit = 0
static void raw_set_pressed(uint8_t *raw, uint8_t id1, bool pressed) {
    const uint8_t byte = (uint8_t)((id1 - 1u) / 8u);
    const uint8_t m = mask_msb(id1);
    if (pressed)
        raw[byte] = (uint8_t)(raw[byte] & ~m);
    else
        raw[byte] = (uint8_t)(raw[byte] | m);
}

static bool edge_is_set(const uint8_t *edges, uint8_t id1) {
    const uint8_t byte = (uint8_t)((id1 - 1u) / 8u);
    return (edges[byte] & mask_msb(id1)) != 0u;
}

/* ============================================================================
 * TESTS
 * ============================================================================ */

void test_debounce_no_edges_when_released() {
    debounce_cfg_t cfg;
    cfg.debounce_ms = 30;
    cfg.count = 10;

    Debouncer d(cfg);
    const uint8_t bytes = (uint8_t)((cfg.count + 7u) / 8u);

    uint8_t raw[2];
    raw_all_released(raw, bytes, cfg.count);

    TEST_ASSERT_FALSE(d.update(raw, bytes, 0));

    uint8_t edges[2] = {0xAA, 0xAA};
    d.getPressedEdges(edges, bytes);
    TEST_ASSERT_EQUAL_UINT8(0x00, edges[0]);
    TEST_ASSERT_EQUAL_UINT8(0x00, edges[1]);
}

void test_debounce_single_press_edge_after_time() {
    debounce_cfg_t cfg;
    cfg.debounce_ms = 30;
    cfg.count = 10;

    Debouncer d(cfg);
    const uint8_t bytes = (uint8_t)((cfg.count + 7u) / 8u);

    uint8_t raw[2];
    raw_all_released(raw, bytes, cfg.count);

    // press id=3 at t=0
    raw_set_pressed(raw, 3, true);

    // within debounce => no edge
    TEST_ASSERT_FALSE(d.update(raw, bytes, 0));
    TEST_ASSERT_FALSE(d.update(raw, bytes, 10));

    // after debounce => edge
    TEST_ASSERT_TRUE(d.update(raw, bytes, 40));

    uint8_t edges[2] = {0, 0};
    d.getPressedEdges(edges, bytes);
    TEST_ASSERT_TRUE(edge_is_set(edges, 3));

    // held => no further edges
    TEST_ASSERT_FALSE(d.update(raw, bytes, 100));
    d.getPressedEdges(edges, bytes);
    TEST_ASSERT_EQUAL_UINT8(0x00, edges[0]);
    TEST_ASSERT_EQUAL_UINT8(0x00, edges[1]);

    // stable pressed should be true
    TEST_ASSERT_TRUE(d.stablePressed(3));
}

void test_debounce_ignores_invalid_bits_in_last_byte() {
    debounce_cfg_t cfg;
    cfg.debounce_ms = 0;
    cfg.count = 10; // last byte valid bits: bits7..6 only

    Debouncer d(cfg);
    const uint8_t bytes = (uint8_t)((cfg.count + 7u) / 8u);

    uint8_t raw[2];
    raw_all_released(raw, bytes, cfg.count);

    // Force invalid bits (lower 6 bits in last byte) to 0 (=pressed)
    raw[1] = (uint8_t)(raw[1] & 0xC0u);

    TEST_ASSERT_FALSE(d.update(raw, bytes, 0));

    uint8_t edges[2] = {0, 0};
    d.getPressedEdges(edges, bytes);

    TEST_ASSERT_EQUAL_UINT8(0x00, edges[0]);
    TEST_ASSERT_EQUAL_UINT8(0x00, edges[1]);
}

/* ============================================================================
 * ARDUINO ENTRY
 * ============================================================================ */

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_debounce_no_edges_when_released);
    RUN_TEST(test_debounce_single_press_edge_after_time);
    RUN_TEST(test_debounce_ignores_invalid_bits_in_last_byte);
    UNITY_END();
}

void loop() {}
