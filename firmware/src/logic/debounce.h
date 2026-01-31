/**
 * @file debounce.h
 * @brief Debounce-Logik fuer active-low, MSB-first Button-Bytes.
 *
 * Mapping: ID 1 -> Byte0 Bit7, ID 8 -> Byte0 Bit0, ID 9 -> Byte1 Bit7, ...
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "hw/io_counts.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * TYPES
 * ============================================================================ */

/**
 * @brief Konfiguration fuer den Debouncer.
 */
typedef struct debounce_cfg_t {
    uint32_t debounce_ms; /**< Entprellzeit in Millisekunden */
    uint8_t count;        /**< Anzahl Buttons */
} debounce_cfg_t;

#ifdef __cplusplus
}
#endif

/* ============================================================================
 * C++ KLASSE
 * ============================================================================ */

#ifdef __cplusplus

/**
 * @brief Debouncer Klasse (C++).
 *
 * Verarbeitet rohe Button-Bytes und erzeugt entprellte Press-Edges.
 */
class Debouncer {
  public:
    /**
     * @brief Konstruktor.
     * @param cfg  Konfiguration
     */
    explicit Debouncer(const debounce_cfg_t &cfg);

    /**
     * @brief Update mit raw bytes (active-low, MSB-first).
     *
     * @param raw       Rohe Button-Bytes
     * @param raw_bytes Anzahl Bytes
     * @param now_ms    Aktuelle Zeit in Millisekunden
     * @return          true wenn mindestens ein Press-Edge aufgetreten
     */
    bool update(const uint8_t *raw, size_t raw_bytes, uint32_t now_ms);

    /**
     * @brief Kopiert Press-Edges in den Ausgabe-Buffer.
     *
     * @param out       Ausgabe-Buffer (MSB-first)
     * @param out_bytes Groesse des Buffers
     */
    void getPressedEdges(uint8_t *out, size_t out_bytes) const;

    /**
     * @brief Prueft ob eine ID stabil gedrueckt ist.
     *
     * @param id1  Button-ID (1-basiert)
     * @return     true wenn stabil gedrueckt
     */
    bool stablePressed(uint8_t id1) const;

  private:
    static inline uint8_t _mask_msb(uint8_t id1) {
        return (uint8_t)(1u << (7u - ((id1 - 1u) % 8u)));
    }

    static inline bool _raw_pressed_msb(const uint8_t *raw, uint8_t id1) {
        if (!raw || id1 == 0)
            return false;
        const uint8_t byte = (uint8_t)((id1 - 1u) / 8u);
        return (raw[byte] & _mask_msb(id1)) == 0u; // active-low: 0 = pressed
    }

    static inline uint8_t _bytes_for(uint8_t count) {
        return (uint8_t)((count + 7u) / 8u);
    }

    static inline uint8_t _last_mask_msb(uint8_t count) {
        const uint8_t rem = (uint8_t)(count % 8u);
        if (rem == 0)
            return 0xFFu;
        // MSB-first: gueltig sind die oberen rem Bits
        return (uint8_t)(0xFFu << (8u - rem));
    }

    debounce_cfg_t _cfg;

    bool _last_raw_pressed[HW::BTN_COUNT];
    bool _stable_pressed[HW::BTN_COUNT];
    uint32_t _last_change_ms[HW::BTN_COUNT];

    uint8_t _pressed_edges[HW::BTN_BYTES];
};

#endif // __cplusplus
