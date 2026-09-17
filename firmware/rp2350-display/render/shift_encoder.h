#pragma once

#include <stdint.h>

#include "led_map.h"

// 8 is the hard ceiling since brightness levels are stored as uint8_t (0-255)
#define BCM_BITS_MAX 8


// Precomputes the (shift-cycle, bit-mask) pair for every (led, channel) into an LUT
void shift_encoder_init(void);

/*
Encodes one column's levels into lhs_column_words/rhs_column_words: one 16-cycle
plane_words array (column_words[plane]) per BCM bit-plane for both wings.
LHS and RHS get the same set of bit-planes (same spatial column, wings
180 deg apart) but different words, since each wing's LED chips are wired up
differently.
*/
void shift_encoder_encode(const uint8_t column_levels[LEDS_PER_WING][CHANNELS], int bcm_bits,
                           uint16_t lhs_column_words[][16], uint16_t rhs_column_words[][16]);
