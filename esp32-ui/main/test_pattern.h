#pragma once

#include <stdint.h>

#include "frame_geometry.h"

// Fills the whole frame with one solid colour
void test_pattern_build(uint8_t frame[NUM_COLUMNS][LEDS_PER_WING][CHANNELS], uint8_t rgb[CHANNELS]);
