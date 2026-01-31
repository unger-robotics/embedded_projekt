/**
 * @file timing_params.h
 * @brief Timing-Konstanten fuer Treiber (CD4021, HC595).
 *
 * Zentralisiert Magic Numbers aus den Treiber-Implementierungen.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CD4021 Timing (Mikrosekunden)
 * ============================================================================
 */

/** Setup-Zeit: P/S HIGH vor Wechsel zu LOW (Parallel Load) */
#define CD4021_PS_SETUP_US 5

/** Hold-Zeit: nach P/S LOW vor SPI-Transfer */
#define CD4021_PS_HOLD_US 2

/* ============================================================================
 * HC595 Timing (Mikrosekunden)
 * ============================================================================
 */

/** Latch-Puls Dauer: RCK HIGH Pulsbreite */
#define HC595_LATCH_PULSE_US 1

#ifdef __cplusplus
}
#endif
