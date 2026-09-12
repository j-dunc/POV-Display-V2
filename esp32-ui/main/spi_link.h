#pragma once

#include <stdbool.h>


// ESP32 side of the RP2350<->ESP32 SPI link. ESP32 is the SPI master.

// Configures SPI2_HOST as master on GPIO2/6/7/10
void spi_link_init(void);

// Runs one transaction and checks the received byte
// Logs result
bool spi_link_echo_test_check(void);
