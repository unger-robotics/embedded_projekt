/**
 * @file selection.cpp
 * @brief One-Hot Auswahl-Logik Implementation.
 */

#include "logic/selection.h"

/* ============================================================================
 * PRIVATE FUNKTIONEN
 * ============================================================================
 */

/**
 * @brief Berechnet Bit-Maske fuer ID (MSB-first).
 */
static inline uint8_t mask_msb(uint8_t id1) {
    return (uint8_t)(1u << (7u - ((id1 - 1u) % 8u)));
}

/**
 * @brief Berechnet benoetigte Bytes fuer count IDs.
 */
static inline uint8_t bytes_for(uint8_t count) {
    return (uint8_t)((count + 7u) / 8u);
}

/* ============================================================================
 * OEFFENTLICHE FUNKTIONEN
 * ============================================================================
 */

uint8_t selection_pick(const uint8_t *pressed_edges, size_t bytes,
                       const selection_cfg_t *cfg) {
    if (!pressed_edges || !cfg)
        return 0;
    if (cfg->count == 0)
        return 0;

    const uint8_t need = bytes_for(cfg->count);
    if (bytes < need)
        return 0;

    uint8_t candidate = 0;

    if (cfg->policy == SELECTION_POLICY_LOWEST_ID_WINS) {
        for (uint8_t id = 1; id <= cfg->count; ++id) {
            const uint8_t byte_idx = (uint8_t)((id - 1u) / 8u);
            if ((pressed_edges[byte_idx] & mask_msb(id)) != 0u) {
                candidate = id;
                break;
            }
        }
    } else {
        // SELECTION_POLICY_LAST_ID_WINS
        for (uint8_t id = 1; id <= cfg->count; ++id) {
            const uint8_t byte_idx = (uint8_t)((id - 1u) / 8u);
            if ((pressed_edges[byte_idx] & mask_msb(id)) != 0u) {
                candidate = id;
            }
        }
    }

    return candidate;
}

uint8_t selection_update(selection_state_t *st, const uint8_t *pressed_edges,
                         size_t bytes, const selection_cfg_t *cfg,
                         uint8_t *out_selected) {
    if (out_selected)
        *out_selected = 0;

    if (!st || !pressed_edges || !cfg || cfg->count == 0)
        return 0;

    const uint8_t need = bytes_for(cfg->count);
    if (bytes < need)
        return 0;

    const uint8_t cand = selection_pick(pressed_edges, bytes, cfg);

    if (cand != 0 && cand != st->selected_id) {
        st->selected_id = cand;
        if (out_selected)
            *out_selected = cand;
        return 1;
    }
    return 0;
}
