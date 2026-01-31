/**
 * @file io_pipeline.cpp
 * @brief I/O Pipeline Implementation.
 */

#include "logic/io_pipeline.h"

#include <cstring>

namespace io_pipeline {

/* ============================================================================
 * PRIVATE FUNKTIONEN
 * ============================================================================
 */

static inline uint8_t clamp_count(uint8_t c) {
    if (c == 0u)
        c = 1u;
    if (c > HW::BTN_COUNT)
        c = HW::BTN_COUNT;
    return c;
}

/**
 * @brief MSB-first Bit-Maske fuer ID.
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
 * OEFFENTLICHE METHODEN
 * ============================================================================
 */

Pipeline::Pipeline(const pipeline_cfg_t &cfg)
    : _cfg(cfg),
      _debouncer(debounce_cfg_t{cfg.debounce_ms, clamp_count(cfg.count)}),
      _sel(selection_state_t{0}) {

    _cfg.count = clamp_count(_cfg.count);

    for (uint8_t i = 0; i < HW::BTN_COUNT; ++i) {
        _guard_armed[i] = false;
        _last_accepted_press_ms[i] = 0;
    }
    std::memset(_edges, 0x00, sizeof(_edges));
}

bool Pipeline::update(const uint8_t *raw, size_t raw_bytes, uint32_t now_ms,
                      uint8_t &out_selected) {
    out_selected = 0;
    if (!raw)
        return false;

    const uint8_t n_bytes = bytes_for(_cfg.count);
    if (raw_bytes < n_bytes)
        return false;

    // 1) Debounce -> pressed edges
    _debouncer.update(raw, n_bytes, now_ms);
    std::memset(_edges, 0x00, sizeof(_edges));
    _debouncer.getPressedEdges(_edges, n_bytes);

    // 2) Press-Guard: filter edges per button
    if (_cfg.press_guard_ms != 0u) {
        for (uint8_t id = 1; id <= _cfg.count; ++id) {
            const uint8_t idx = (uint8_t)(id - 1u);
            const uint8_t byte_idx = (uint8_t)((id - 1u) / 8u);
            const uint8_t m = mask_msb(id);

            if ((_edges[byte_idx] & m) == 0u)
                continue; // no edge

            // guard only if armed already (first press always allowed)
            if (_guard_armed[idx]) {
                const uint32_t dt = now_ms - _last_accepted_press_ms[idx];
                if (dt < _cfg.press_guard_ms) {
                    _edges[byte_idx] = (uint8_t)(_edges[byte_idx] & ~m);
                    continue;
                }
            }

            _last_accepted_press_ms[idx] = now_ms;
            _guard_armed[idx] = true;
        }
    }

    // 3) Selection (policy) + change detection
    selection_cfg_t sel_cfg;
    sel_cfg.count = _cfg.count;
    sel_cfg.policy = _cfg.policy;

    uint8_t new_sel = 0;
    if (selection_update(&_sel, _edges, n_bytes, &sel_cfg, &new_sel)) {
        out_selected = new_sel;
        return true;
    }
    return false;
}

void Pipeline::force_selected(uint8_t id) {
    if (id > _cfg.count)
        id = 0;
    _sel.selected_id = id;
}

} // namespace io_pipeline
