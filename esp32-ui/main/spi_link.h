#pragma once

#include <stdbool.h>

#include "spi_protocol.h"

// ESP32 side of the RP2350<->ESP32 SPI link. ESP32 is the SPI master.

// Configures SPI2_HOST as master on GPIO2/6/7/10
void spi_link_init(void);

// Note that for each command that expects a response,
// The return value is a bool whether succesful
// The output is written to the pointer given to the function

// Sends CMD_PING, waits for the RP2350 to echo it back. Logs result.
bool spi_link_ping(void);

// Sends CMD_GET_STATUS, fills out on success. Logs result.
bool spi_link_get_status(spi_status_t *out);

// Pushes one column of raw 0-255 RGB to the RP2350
bool spi_link_push_column(uint16_t col, const uint8_t rgb[LEDS_PER_WING][CHANNELS]);

// Pushes an entire frame, one column at a time. Returns false if a push fails
bool spi_link_push_frame(const uint8_t frame[NUM_COLUMNS][LEDS_PER_WING][CHANNELS]);

// Sends CMD_SET_CALIBRATION - update RP2350 colour calibration + max duty fraction, and redraw image
bool spi_link_set_calibration(float r_scale, float g_scale, float b_scale, float gamma, float radial_gamma, float max_duty_fraction, bool dither_enabled, float bcm_bits);

// Sends CMD_GET_CALIBRATION, fills out with the RP2350's current live calibration values
bool spi_link_get_calibration(calibration_payload_t *out);
