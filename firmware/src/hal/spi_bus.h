/**
 * @file spi_bus.h
 * @brief SPI-Bus Abstraktion mit C-Interface.
 *
 * Haelt Board-spezifische SPI-Pin-Zuordnung an einer Stelle.
 * Ermoeglicht effiziente Multi-Byte-Transaktionen.
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
 * @brief SPI-Einstellungen (Frequenz, Bit-Order, Modus).
 */
typedef struct spi_settings_t {
    uint32_t freq_hz;   /**< SPI-Taktfrequenz in Hz */
    uint8_t bit_order;  /**< MSBFIRST (1) oder LSBFIRST (0) */
    uint8_t mode;       /**< SPI_MODE0..SPI_MODE3 */
} spi_settings_t;

/**
 * @brief SPI-Bus Handle (opak, intern verwaltet).
 *
 * Derzeit ein leerer Struct, da nur ein globaler SPI-Bus existiert.
 * Ermoeglicht spaetere Erweiterung auf mehrere Busse.
 */
typedef struct spi_bus_t {
    uint8_t _reserved; /**< Platzhalter fuer zukuenftige Erweiterung */
} spi_bus_t;

/* ============================================================================
 * OEFFENTLICHE FUNKTIONEN
 * ============================================================================ */

/**
 * @brief Initialisiert den SPI-Bus mit den angegebenen Pins.
 *
 * @param bus   Zeiger auf spi_bus_t Instanz
 * @param sck   SCK Pin
 * @param miso  MISO Pin
 * @param mosi  MOSI Pin
 */
void spi_bus_begin(spi_bus_t *bus, int sck, int miso, int mosi);

/**
 * @brief Startet eine SPI-Transaktion mit den angegebenen Einstellungen.
 *
 * @param bus   Zeiger auf spi_bus_t Instanz
 * @param s     Zeiger auf spi_settings_t
 */
void spi_bus_begin_transaction(spi_bus_t *bus, const spi_settings_t *s);

/**
 * @brief Beendet die aktive SPI-Transaktion.
 *
 * @param bus   Zeiger auf spi_bus_t Instanz
 */
void spi_bus_end_transaction(spi_bus_t *bus);

/**
 * @brief Transferiert ein einzelnes Byte.
 *
 * @param bus   Zeiger auf spi_bus_t Instanz
 * @param v     Zu sendendes Byte
 * @return      Empfangenes Byte
 */
uint8_t spi_bus_transfer(spi_bus_t *bus, uint8_t v);

/**
 * @brief Transferiert einen Buffer (innerhalb aktiver Transaktion).
 *
 * @param bus   Zeiger auf spi_bus_t Instanz
 * @param tx    Sende-Buffer (kann NULL sein fuer nur-Empfang)
 * @param rx    Empfangs-Buffer (kann NULL sein fuer nur-Senden)
 * @param n     Anzahl Bytes
 */
void spi_bus_transfer_buffer(spi_bus_t *bus, const uint8_t *tx, uint8_t *rx,
                             size_t n);

/**
 * @brief Convenience: Komplette Transaktion in einem Aufruf.
 *
 * @param bus   Zeiger auf spi_bus_t Instanz
 * @param s     Zeiger auf spi_settings_t
 * @param tx    Sende-Buffer (kann NULL sein)
 * @param rx    Empfangs-Buffer (kann NULL sein)
 * @param n     Anzahl Bytes
 */
void spi_bus_transfer_full(spi_bus_t *bus, const spi_settings_t *s,
                           const uint8_t *tx, uint8_t *rx, size_t n);

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
 * @brief Legacy C++ Wrapper fuer spi_bus_t.
 *
 * Ermoeglicht schrittweise Migration bestehenden Codes.
 */
class SpiBus {
  public:
    SpiBus() : _bus{} {}

    void begin(int sck, int miso, int mosi) {
        spi_bus_begin(&_bus, sck, miso, mosi);
    }

    void beginTransaction(const SPISettings &s) {
        spi_settings_t cs;
        cs.freq_hz = s._clock;
        cs.bit_order = s._bitOrder;
        cs.mode = s._dataMode;
        spi_bus_begin_transaction(&_bus, &cs);
    }

    void endTransaction() { spi_bus_end_transaction(&_bus); }

    uint8_t transfer(uint8_t v) { return spi_bus_transfer(&_bus, v); }

    void transferBuffer(const uint8_t *tx, uint8_t *rx, size_t n) {
        spi_bus_transfer_buffer(&_bus, tx, rx, n);
    }

    void transferBuffer(const SPISettings &s, const uint8_t *tx, uint8_t *rx,
                        size_t n) {
        spi_settings_t cs;
        cs.freq_hz = s._clock;
        cs.bit_order = s._bitOrder;
        cs.mode = s._dataMode;
        spi_bus_transfer_full(&_bus, &cs, tx, rx, n);
    }

    spi_bus_t *handle() { return &_bus; }

  private:
    spi_bus_t _bus;
};
#endif
