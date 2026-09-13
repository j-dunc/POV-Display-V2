#pragma once

#include <stdint.h>

#include "frame_geometry.h"
#include "frame_buffer.h"

/*
Contains a 16-cycle word array per BCM bit-plane, per column.
Built before render so no build timing cost at render time.
Note these are different from LHS to RHS only because bit layout differs per wing.
Column-Frame mapping is identical between LHS and RHS because column identifies a fixed angular position.
Difference in wing positions is handled by render loop.
*/
extern uint16_t image_lhs_words[NUM_COLUMNS][BCM_BITS][16];
extern uint16_t image_rhs_words[NUM_COLUMNS][BCM_BITS][16];

// Build test image - red brightness ramps 0 to BCM_MAX_LEVEL once per revolution
void image_source_build_ramp(void);

// Build test image - a colour wheel stepping through PALETTE in equal bands
void image_source_build_palette(void);

// Build test image - a smooth rainbow, one full turn of hue around the circle
void image_source_build_rainbow(void);
