#pragma once

#include <stdint.h>

#include "shift_encoder.h"


// Double-buffered storage for one full frame (all NUM_COLUMNS columns, both wings)
// of encoded shift words.
extern uint16_t (*frame_lhs_words)[BCM_BITS_MAX][16]; // one frame, index by column for that column's words
extern uint16_t (*frame_rhs_words)[BCM_BITS_MAX][16];

// Encodes one column's levels into shift words (via shift_encoder) and stages it in the
// back buffer, marking a full frame ready to swap in once every column has landed
void frame_buffer_publish_column(int col, const uint8_t column_levels[LEDS_PER_WING][CHANNELS], int bcm_bits);

// Swaps the front/back buffers if a full frame has finished landing in the back
// buffer since the last swap. Called by render.c once per rev
void frame_buffer_swap_pending(void);
