/**
 * @file cd4021.h
 * @brief CD4021 Parallel-In/Serial-Out Shift Register Treiber.
 *
 * - pin_ps: P/S Steuerung (HIGH = Parallel Load, LOW = Shift)
 * - pin_miso: Q8 serieller Ausgang
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
 * @brief Konfiguration fuer CD4021 Initialisierung.
 */
typedef struct cd4021_cfg_t {
    int pin_ps;              /**< P/S Steuerpin */
    int pin_miso;            /**< MISO Pin (Q8 Ausgang) */
    spi_settings_t settings; /**< SPI-Einstellungen */
} cd4021_cfg_t;

/**
 * @brief CD4021 Instanz.
 */
typedef struct cd4021_t {
    spi_bus_t *_bus;         /**< SPI-Bus Handle */
    int _pin_ps;             /**< P/S Steuerpin */
    int _pin_miso;           /**< MISO Pin */
    spi_settings_t _settings; /**< SPI-Einstellungen */
} cd4021_t;

/* ============================================================================
 * OEFFENTLICHE FUNKTIONEN
 * ============================================================================ */

/**
 * @brief Initialisiert eine CD4021 Instanz.
 *
 * @param dev   Zeiger auf cd4021_t Instanz
 * @param bus   Zeiger auf spi_bus_t
 * @param cfg   Zeiger auf Konfiguration
 */
void cd4021_init(cd4021_t *dev, spi_bus_t *bus, const cd4021_cfg_t *cfg);

/**
 * @brief Konfiguriert die GPIO-Pins fuer den CD4021.
 *
 * @param dev   Zeiger auf cd4021_t Instanz
 */
void cd4021_begin(cd4021_t *dev);

/**
 * @brief Liest n Bytes vom CD4021 (raw bits, active-low).
 *
 * @param dev   Zeiger auf cd4021_t Instanz
 * @param out   Ausgabe-Buffer
 * @param n     Anzahl Bytes
 *
 * @note Mit SPI_MODE1 (CPHA=1) wird das erste Bit vor dem Takten gelesen,
 *       da die erste Taktflanke den CD4021 weiterschiebt bevor SPI sampelt.
 */
void cd4021_read(cd4021_t *dev, uint8_t *out, size_t n);

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
 * @brief Legacy C++ Wrapper fuer cd4021_t.
 */
class CD4021 {
  public:
    CD4021(SpiBus &bus, int pin_ps, int pin_miso, SPISettings settings)
        : _legacy_bus(&bus) {
        cd4021_cfg_t cfg;
        cfg.pin_ps = pin_ps;
        cfg.pin_miso = pin_miso;
        cfg.settings.freq_hz = settings._clock;
        cfg.settings.bit_order = settings._bitOrder;
        cfg.settings.mode = settings._dataMode;
        cd4021_init(&_dev, bus.handle(), &cfg);
    }

    void begin() { cd4021_begin(&_dev); }

    void read(uint8_t *out, size_t n) { cd4021_read(&_dev, out, n); }

  private:
    SpiBus *_legacy_bus;
    cd4021_t _dev;
};
#endif
