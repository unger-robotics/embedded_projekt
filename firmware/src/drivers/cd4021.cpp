/**
 * @file cd4021.cpp
 * @brief CD4021 Treiber Implementation.
 */

#include "cd4021.h"
#include "hw/timing_params.h"

#include <Arduino.h>

/* ============================================================================
 * OEFFENTLICHE FUNKTIONEN
 * ============================================================================
 */

extern "C" void cd4021_init(cd4021_t *dev, spi_bus_t *bus,
                            const cd4021_cfg_t *cfg) {
    if (!dev || !bus || !cfg)
        return;

    dev->_bus = bus;
    dev->_pin_ps = cfg->pin_ps;
    dev->_pin_miso = cfg->pin_miso;
    dev->_settings = cfg->settings;
}

extern "C" void cd4021_begin(cd4021_t *dev) {
    if (!dev)
        return;

    pinMode(dev->_pin_ps, OUTPUT);
    pinMode(dev->_pin_miso, INPUT);
    digitalWrite(dev->_pin_ps, LOW); // Shift-Modus als Ruhezustand
}

extern "C" void cd4021_read(cd4021_t *dev, uint8_t *out, size_t n) {
    if (!dev || !out || n == 0)
        return;

    // Parallel Load
    digitalWrite(dev->_pin_ps, HIGH);
    delayMicroseconds(CD4021_PS_SETUP_US);
    digitalWrite(dev->_pin_ps, LOW);
    delayMicroseconds(CD4021_PS_HOLD_US);

    // Erstes Bit vor dem Takten lesen (MODE1 Kompensation)
    const uint8_t firstBit = digitalRead(dev->_pin_miso) ? 1u : 0u;

    spi_bus_begin_transaction(dev->_bus, &dev->_settings);

    uint8_t carryIn = (uint8_t)(firstBit << 7);
    for (size_t i = 0; i < n; ++i) {
        const uint8_t r = spi_bus_transfer(dev->_bus, 0x00);

        // r Bits (MSBFIRST) repraesentieren b1..b8; rechts schieben und
        // gesampletes b0 einfuegen
        out[i] = (uint8_t)((r >> 1) | carryIn);

        // Naechstes Byte bekommt aktuelles LSB (b8) als MSB carry-in
        carryIn = (uint8_t)((r & 0x01u) << 7);
    }

    spi_bus_end_transaction(dev->_bus);
}
