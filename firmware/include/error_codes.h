/**
 * @file error_codes.h
 * @brief Zentrale Fehlercode-Definition fuer die gesamte Firmware.
 *
 * APP_ERR_-Prefix vermeidet Kollision mit lwIP und anderen Libraries.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * TYPES
 * ============================================================================ */

/**
 * @brief Fehlercode-Enumeration.
 *
 * Erfolg ist 0, Fehler sind negative Werte.
 */
typedef enum error_code_e {
    APP_ERR_SUCCESS = 0,       /**< Kein Fehler */
    APP_ERR_INVALID_ARG = -1,  /**< Ungueltiges Argument */
    APP_ERR_NULL_PTR = -2,     /**< Nullpointer uebergeben */
    APP_ERR_BUFFER_TOO_SMALL = -3, /**< Buffer zu klein */
    APP_ERR_TIMEOUT = -4,      /**< Timeout aufgetreten */
    APP_ERR_NOT_INIT = -5,     /**< Modul nicht initialisiert */
} error_code_e;

#ifdef __cplusplus
}
#endif
