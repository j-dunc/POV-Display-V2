#pragma once

// RP2350 side of the RP2350<->ESP32 SPI link

// Configures SPI1 as slave on the GPIO12-15 hardware SPI pins
void spi_link_init(void);

// Blocks forever, continuously shifting out SPI_LINK_ECHO_TEST_BYTE
void spi_link_echo_test(void);
