#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "frame_geometry.h"
#include "frame_buffer.h"
#include "spi_protocol.h"

/*
Contains a 16-cycle word array per BCM bit-plane, per column.
Sized to max BCM bits per column
Built before render so no build timing cost at render time.
Note these are different from LHS to RHS only because bit layout differs per wing.
Column-Frame mapping is identical between LHS and RHS because column identifies a fixed angular position.
Difference in wing positions is handled by render loop.
*/
extern uint16_t image_lhs_words[NUM_COLUMNS][BCM_BITS_MAX][16];
extern uint16_t image_rhs_words[NUM_COLUMNS][BCM_BITS_MAX][16];

// (Re)builds the calibration lookup tables from the current calibration values.
// Reduces per frame computational cost
void rebuild_calibration(void);

// Test pattern - one channel (RED/GREEN/BLUE) ramps 0 to full scale
void image_source_build_channel_ramp(int channel);

// Test pattern - all channels ramp together 0 to full scale
void image_source_build_gray_ramp(void);

// Test pattern - flat full-scale white everywhere
void image_source_build_radial_white(void);

// Test pattern - a smooth rainbow, one full turn of hue around the circle
void image_source_build_rainbow(void);

// Displays one TEST_PATTERN_* id
void image_source_show_test_pattern(uint8_t pattern_id);

// Sets one column from raw 0-255 RGB, e.g. pushed live over SPI from the ESP32
void image_source_set_column(int col, const uint8_t rgb[LEDS_PER_WING][CHANNELS]);

// Updates the live colour calibration (per-channel balance scale, gamma,
// radial falloff correction, ordered dithering, and BCM bit depth) and
// redraws. bcm_bits is clamped to [1, BCM_BITS_MAX]
void image_source_set_calibration(float r_scale, float g_scale, float b_scale, float gamma, float radial_gamma, bool dither_enabled, int bcm_bits);

// Reads back the current live colour calibration
void image_source_get_calibration(calibration_payload_t *out);

// Reads back the current active BCM bit depth
int image_source_get_bcm_bits(void);
