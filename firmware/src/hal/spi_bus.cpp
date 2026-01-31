/**
 * @file spi_bus.cpp
 * @brief SPI-Bus Implementation (Arduino SPI Backend).
 */

#include "spi_bus.h"

#include <Arduino.h>
#include <SPI.h>

/* ============================================================================
 * OEFFENTLICHE FUNKTIONEN
 * ============================================================================
 */

extern "C" void spi_bus_begin(spi_bus_t *bus, int sck, int miso, int mosi) {
    (void)bus;
    SPI.begin(sck, miso, mosi);
}

extern "C" void spi_bus_begin_transaction(spi_bus_t *bus,
                                          const spi_settings_t *s) {
    (void)bus;
    if (!s)
        return;
    SPI.beginTransaction(SPISettings(s->freq_hz, s->bit_order, s->mode));
}

extern "C" void spi_bus_end_transaction(spi_bus_t *bus) {
    (void)bus;
    SPI.endTransaction();
}

extern "C" uint8_t spi_bus_transfer(spi_bus_t *bus, uint8_t v) {
    (void)bus;
    return SPI.transfer(v);
}

extern "C" void spi_bus_transfer_buffer(spi_bus_t *bus, const uint8_t *tx,
                                        uint8_t *rx, size_t n) {
    (void)bus;
    for (size_t i = 0; i < n; ++i) {
        const uint8_t v = tx ? tx[i] : 0x00;
        const uint8_t r = SPI.transfer(v);
        if (rx)
            rx[i] = r;
    }
}

extern "C" void spi_bus_transfer_full(spi_bus_t *bus, const spi_settings_t *s,
                                      const uint8_t *tx, uint8_t *rx,
                                      size_t n) {
    spi_bus_begin_transaction(bus, s);
    spi_bus_transfer_buffer(bus, tx, rx, n);
    spi_bus_end_transaction(bus);
}
