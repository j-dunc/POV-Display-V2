#pragma once

#include <stdbool.h>

#include "spi_protocol.h"

// ESP32 side of the RP2350<->ESP32 SPI link. ESP32 is the SPI master.

// Configures SPI2_HOST as master on GPIO2/6/7/10
void spi_link_init(void);


// Sends CMD_PING, waits for the RP2350 to echo it back. Logs result.
bool spi_link_ping(void);

// Sends CMD_GET_STATUS, fills out on success. Logs result.
bool spi_link_get_status(spi_status_t *out);
