/**
 * @file io_pipeline.h
 * @brief I/O Pipeline: Debounce + Press-Guard + One-Hot Selection.
 *
 * Kombiniert Entprellung, Press-Guard und Auswahl-Logik in einer Pipeline.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "hw/io_counts.h"
#include "logic/debounce.h"
#include "logic/selection.h"

#ifdef __cplusplus

namespace io_pipeline {

/* ============================================================================
 * TYPES
 * ============================================================================ */

/**
 * @brief Pipeline-Konfiguration.
 */
struct pipeline_cfg_t {
    uint32_t debounce_ms;        /**< Entprellzeit in ms */
    uint32_t press_guard_ms;     /**< Guard-Zeit zwischen Druecken in ms */
    selection_policy_e policy;   /**< Auswahl-Policy */
    uint8_t count;               /**< Anzahl Buttons */

    explicit constexpr pipeline_cfg_t(
        uint32_t debounce = 30, uint32_t guard = 30,
        selection_policy_e p = SELECTION_POLICY_LOWEST_ID_WINS,
        uint8_t c = HW::BTN_COUNT)
        : debounce_ms(debounce), press_guard_ms(guard), policy(p), count(c) {}
};

/* ============================================================================
 * PIPELINE KLASSE
 * ============================================================================ */

/**
 * @brief I/O Pipeline Klasse.
 *
 * Kombiniert Debouncer und Selection-Logik mit zusaetzlichem Press-Guard.
 */
class Pipeline {
  public:
    /**
     * @brief Konstruktor.
     * @param cfg  Pipeline-Konfiguration
     */
    explicit Pipeline(const pipeline_cfg_t &cfg);

    /**
     * @brief Verarbeitet raw Button-Daten.
     *
     * @param raw           Rohe Button-Bytes (active-low, MSB-first)
     * @param raw_bytes     Anzahl Bytes
     * @param now_ms        Aktuelle Zeit in ms
     * @param out_selected  Ausgabe: neue ID bei Aenderung (0..count)
     * @return              true wenn Auswahl sich geaendert hat
     */
    bool update(const uint8_t *raw, size_t raw_bytes, uint32_t now_ms,
                uint8_t &out_selected);

    /**
     * @brief Gibt aktuelle Auswahl zurueck.
     * @return  Ausgewaehlte ID (0 = keine, 1..count)
     */
    uint8_t selected() const { return _sel.selected_id; }

    /**
     * @brief Erzwingt eine Auswahl (Host-Preempt).
     * @param id  Neue ID (0..count, 0 = alle aus)
     */
    void force_selected(uint8_t id);

  private:
    pipeline_cfg_t _cfg;

    Debouncer _debouncer;
    selection_state_t _sel;

    bool _guard_armed[HW::BTN_COUNT];
    uint32_t _last_accepted_press_ms[HW::BTN_COUNT];

    uint8_t _edges[HW::BTN_BYTES];
};

} // namespace io_pipeline

#endif // __cplusplus
