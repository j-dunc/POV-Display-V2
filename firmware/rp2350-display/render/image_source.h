#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "frame_geometry.h"
#include "frame_buffer.h"
#include "spi_protocol.h"

// (Re)builds the calibration lookup tables from the current calibration values.
// Reduces per frame computational cost
void rebuild_calibration(void);

// Converts one raw 0-255 channel value, at a given radial LED position and column, to
// a 0-bcm_max_level level - applying gamma, channel, and radial balance and bayer dithering
uint8_t image_source_apply_calibration(uint8_t raw_255, int channel, int led, int col);

// Displays one TEST_PATTERN_* id
void image_source_show_test_pattern(uint8_t pattern_id);

// Sets one column from raw 0-255 RGB, e.g. pushed live over SPI from the ESP32
void image_source_set_column(int col, const uint8_t rgb[LEDS_PER_WING][CHANNELS]);

// Updates the live colour calibration (per-channel balance scale, gamma,
// radial falloff correction, max duty fraction, ordered dithering, and BCM
// bit depth) and redraws. bcm_bits is clamped to [1, BCM_BITS_MAX]
void image_source_set_calibration(float r_scale, float g_scale, float b_scale, float gamma, float radial_gamma, float max_duty_fraction, bool dither_enabled, int bcm_bits);

// Reads back the current live colour calibration
void image_source_get_calibration(calibration_payload_t *out);

// Reads back the current active BCM bit depth
int image_source_get_bcm_bits(void);

// Reads back the current max duty fraction
float image_source_get_max_duty_fraction(void);
