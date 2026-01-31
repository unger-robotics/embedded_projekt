/**
 * @file debounce.cpp
 * @brief Debounce-Logik Implementation.
 */

#include "logic/debounce.h"

#include <cstring>

/* ============================================================================
 * OEFFENTLICHE METHODEN
 * ============================================================================
 */

Debouncer::Debouncer(const debounce_cfg_t &cfg) : _cfg(cfg) {
    if (_cfg.count == 0)
        _cfg.count = 1;
    if (_cfg.count > HW::BTN_COUNT)
        _cfg.count = HW::BTN_COUNT;

    for (uint8_t i = 0; i < HW::BTN_COUNT; ++i) {
        _last_raw_pressed[i] = false;
        _stable_pressed[i] = false;
        _last_change_ms[i] = 0;
    }
    std::memset(_pressed_edges, 0x00, sizeof(_pressed_edges));
}

bool Debouncer::update(const uint8_t *raw, size_t raw_bytes, uint32_t now_ms) {
    const uint8_t n_bytes = _bytes_for(_cfg.count);

    // Immer auf "keine edges" setzen (auch bei early return)
    std::memset(_pressed_edges, 0x00, sizeof(_pressed_edges));

    if (!raw || raw_bytes < n_bytes)
        return false;

    bool any_edge = false;

    for (uint8_t id = 1; id <= _cfg.count; ++id) {
        const uint8_t idx = (uint8_t)(id - 1u);
        const bool rp = _raw_pressed_msb(raw, id);

        if (rp != _last_raw_pressed[idx]) {
            _last_raw_pressed[idx] = rp;
            _last_change_ms[idx] = now_ms;
        }

        if (rp != _stable_pressed[idx]) {
            const uint32_t dt = now_ms - _last_change_ms[idx];
            if (dt >= _cfg.debounce_ms) {
                const bool was_stable_pressed = _stable_pressed[idx];
                _stable_pressed[idx] = rp;

                if (!was_stable_pressed && rp) {
                    const uint8_t byte_idx = (uint8_t)((id - 1u) / 8u);
                    _pressed_edges[byte_idx] =
                        (uint8_t)(_pressed_edges[byte_idx] | _mask_msb(id));
                    any_edge = true;
                }
            }
        }
    }

    _pressed_edges[n_bytes - 1] =
        (uint8_t)(_pressed_edges[n_bytes - 1] & _last_mask_msb(_cfg.count));

    for (uint8_t i = n_bytes; i < HW::BTN_BYTES; ++i)
        _pressed_edges[i] = 0x00;

    return any_edge;
}

void Debouncer::getPressedEdges(uint8_t *out, size_t out_bytes) const {
    const uint8_t n_bytes = _bytes_for(_cfg.count);
    if (!out || out_bytes < n_bytes)
        return;

    std::memcpy(out, _pressed_edges, n_bytes);
    out[n_bytes - 1] = (uint8_t)(out[n_bytes - 1] & _last_mask_msb(_cfg.count));
}

bool Debouncer::stablePressed(uint8_t id1) const {
    if (id1 == 0 || id1 > _cfg.count)
        return false;
    return _stable_pressed[(uint8_t)(id1 - 1u)];
}
