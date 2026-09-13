#pragma once

// RP2350 side of the RP2350<->ESP32 SPI link

// Configures SPI1 as slave on the GPIO12-15 hardware SPI pins
void spi_link_init(void);

// Blocks forever servicing commands from the ESP32.
// Should be handed to core 1 to avoid blocking core 0 rendering
void spi_link_command_loop(void);
