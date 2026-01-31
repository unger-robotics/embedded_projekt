/**
 * @file hc595.h
 * @brief 74HC595 Serial-In/Parallel-Out Shift Register Treiber.
 *
 * - pin_rck: Latch (RCK / STCP)
 * - pin_oe: Output Enable (OE, active-low). -1 wenn nicht verbunden.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "hal/spi_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * TYPES
 * ============================================================================ */

/**
 * @brief Konfiguration fuer HC595 Initialisierung.
 */
typedef struct hc595_cfg_t {
    int pin_rck;             /**< Latch Pin */
    int pin_oe;              /**< Output Enable Pin (-1 = nicht verbunden) */
    spi_settings_t settings; /**< SPI-Einstellungen */
} hc595_cfg_t;

/**
 * @brief HC595 Instanz.
 */
typedef struct hc595_t {
    spi_bus_t *_bus;         /**< SPI-Bus Handle */
    int _pin_rck;            /**< Latch Pin */
    int _pin_oe;             /**< Output Enable Pin */
    spi_settings_t _settings; /**< SPI-Einstellungen */
} hc595_t;

/* ============================================================================
 * OEFFENTLICHE FUNKTIONEN
 * ============================================================================ */

/**
 * @brief Initialisiert eine HC595 Instanz.
 *
 * @param dev   Zeiger auf hc595_t Instanz
 * @param bus   Zeiger auf spi_bus_t
 * @param cfg   Zeiger auf Konfiguration
 */
void hc595_init(hc595_t *dev, spi_bus_t *bus, const hc595_cfg_t *cfg);

/**
 * @brief Konfiguriert die GPIO-Pins fuer den HC595.
 *
 * @param dev           Zeiger auf hc595_t Instanz
 * @param enable_outputs  true = Ausgaenge aktivieren (OE LOW)
 */
void hc595_begin(hc595_t *dev, uint8_t enable_outputs);

/**
 * @brief Schreibt n Bytes und latcht danach.
 *
 * Byte-Reihenfolge: Letztes Byte wird zuerst gesendet (ueblich bei Kaskaden).
 *
 * @param dev   Zeiger auf hc595_t Instanz
 * @param data  Daten-Buffer
 * @param n     Anzahl Bytes
 */
void hc595_write(hc595_t *dev, const uint8_t *data, size_t n);

#ifdef __cplusplus
}
#endif

/* ============================================================================
 * C++ KOMPATIBILITAET (Legacy-Wrapper)
 * ============================================================================ */

#ifdef __cplusplus
#include <Arduino.h>
#include <SPI.h>

/**
 * @brief Legacy C++ Wrapper fuer hc595_t.
 */
class HC595 {
  public:
    HC595(SpiBus &bus, int pin_rck, int pin_oe, SPISettings settings)
        : _legacy_bus(&bus) {
        hc595_cfg_t cfg;
        cfg.pin_rck = pin_rck;
        cfg.pin_oe = pin_oe;
        cfg.settings.freq_hz = settings._clock;
        cfg.settings.bit_order = settings._bitOrder;
        cfg.settings.mode = settings._dataMode;
        hc595_init(&_dev, bus.handle(), &cfg);
    }

    void begin(bool enableOutputs = true) {
        hc595_begin(&_dev, enableOutputs ? 1 : 0);
    }

    void write(const uint8_t *data, size_t n) { hc595_write(&_dev, data, n); }

  private:
    SpiBus *_legacy_bus;
    hc595_t _dev;
};
#endif
