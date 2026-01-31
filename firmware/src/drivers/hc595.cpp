/**
 * @file hc595.cpp
 * @brief HC595 Treiber Implementation.
 */

#include "hc595.h"
#include "hw/timing_params.h"

#include <Arduino.h>

/* ============================================================================
 * PRIVATE FUNKTIONEN
 * ============================================================================
 */

/**
 * @brief Erzeugt einen Latch-Puls (RCK HIGH -> LOW).
 */
static void hc595_latch(hc595_t *dev) {
    digitalWrite(dev->_pin_rck, HIGH);
    delayMicroseconds(HC595_LATCH_PULSE_US);
    digitalWrite(dev->_pin_rck, LOW);
}

/* ============================================================================
 * OEFFENTLICHE FUNKTIONEN
 * ============================================================================
 */

extern "C" void hc595_init(hc595_t *dev, spi_bus_t *bus,
                           const hc595_cfg_t *cfg) {
    if (!dev || !bus || !cfg)
        return;

    dev->_bus = bus;
    dev->_pin_rck = cfg->pin_rck;
    dev->_pin_oe = cfg->pin_oe;
    dev->_settings = cfg->settings;
}

extern "C" void hc595_begin(hc595_t *dev, uint8_t enable_outputs) {
    if (!dev)
        return;

    pinMode(dev->_pin_rck, OUTPUT);
    digitalWrite(dev->_pin_rck, LOW);

    if (dev->_pin_oe >= 0) {
        pinMode(dev->_pin_oe, OUTPUT);
        // OE ist active-low: LOW = Ausgaenge aktiv
        digitalWrite(dev->_pin_oe, enable_outputs ? LOW : HIGH);
    }
}

extern "C" void hc595_write(hc595_t *dev, const uint8_t *data, size_t n) {
    if (!dev || !data || n == 0)
        return;

    spi_bus_begin_transaction(dev->_bus, &dev->_settings);

    // Kaskaden: Letztes Byte zuerst senden
    for (int i = (int)n - 1; i >= 0; --i) {
        spi_bus_transfer(dev->_bus, data[i]);
    }

    spi_bus_end_transaction(dev->_bus);

    hc595_latch(dev);
}
