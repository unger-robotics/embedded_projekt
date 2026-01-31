/**
 * @file selection.h
 * @brief One-Hot Auswahl-Logik.
 *
 * Verwaltet Kanalauswahl (1..N) basierend auf Button-Press-Edges.
 * selectedId 0 bedeutet "keine Auswahl / alle aus".
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * TYPES
 * ============================================================================ */

/**
 * @brief Auswahl-Zustand.
 */
typedef struct selection_state_t {
    uint8_t selected_id; /**< Aktuell ausgewaehlte ID (0 = keine, 1..N) */
} selection_state_t;

/**
 * @brief Auswahl-Policy bei gleichzeitigen Druecken.
 */
typedef enum selection_policy_e {
    SELECTION_POLICY_LOWEST_ID_WINS = 0, /**< Niedrigste ID gewinnt */
    SELECTION_POLICY_LAST_ID_WINS = 1,   /**< Hoechste ID gewinnt */
} selection_policy_e;

/**
 * @brief Konfiguration fuer die Auswahl-Logik.
 */
typedef struct selection_cfg_t {
    uint8_t count;             /**< Anzahl IDs (1..count) */
    selection_policy_e policy; /**< Auswahl-Policy */
} selection_cfg_t;

/* ============================================================================
 * OEFFENTLICHE FUNKTIONEN
 * ============================================================================ */

/**
 * @brief Ermittelt die zu selektierende ID aus pressed edges.
 *
 * @param pressed_edges  Press-Edges Buffer (MSB-first, Bit=1 = Edge)
 * @param bytes          Anzahl Bytes im Buffer
 * @param cfg            Zeiger auf Konfiguration
 * @return               Ausgewaehlte ID (1..count) oder 0 wenn keine
 */
uint8_t selection_pick(const uint8_t *pressed_edges, size_t bytes,
                       const selection_cfg_t *cfg);

/**
 * @brief Aktualisiert den Auswahl-Zustand.
 *
 * @param st             Zeiger auf Zustand
 * @param pressed_edges  Press-Edges Buffer (MSB-first)
 * @param bytes          Anzahl Bytes
 * @param cfg            Zeiger auf Konfiguration
 * @param out_selected   Ausgabe: neue ID bei Aenderung
 * @return               true wenn Auswahl sich geaendert hat
 */
uint8_t selection_update(selection_state_t *st, const uint8_t *pressed_edges,
                         size_t bytes, const selection_cfg_t *cfg,
                         uint8_t *out_selected);

#ifdef __cplusplus
}
#endif
