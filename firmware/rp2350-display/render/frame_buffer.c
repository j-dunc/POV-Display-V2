// frame_buffer: double-buffered storage for one full frame (all columns, both
// wings) of encoded shift words. Tracks when a full frame has landed and
// swaps it in for render.c to pick up

#include "frame_buffer.h"

#include <stdbool.h>
#include <string.h>

#include "hardware/sync.h"

// Stores the entire frame, shift word array per BCM bit-plane per column. Sized to BCM_BITS_MAX
// only planes 0..bcm_bits-1 are meaningful at any time
// Double buffered so render.c is never reading a half-built frame: frame_*_words is
// the front buffer render.c reads every revolution, back_frame_*_words is what gets
// written here. Once every column has landed in the back buffer since the last swap,
// it's marked ready for render.c to swap in at the next revolution boundary.
static uint16_t buffer_lhs[2][NUM_COLUMNS][BCM_BITS_MAX][16]; // front and back buffers
static uint16_t buffer_rhs[2][NUM_COLUMNS][BCM_BITS_MAX][16];

uint16_t (*frame_lhs_words)[BCM_BITS_MAX][16] = buffer_lhs[0]; // NUM_COLUMNS long
uint16_t (*frame_rhs_words)[BCM_BITS_MAX][16] = buffer_rhs[0];
static uint16_t (*back_frame_lhs_words)[BCM_BITS_MAX][16] = buffer_lhs[1];
static uint16_t (*back_frame_rhs_words)[BCM_BITS_MAX][16] = buffer_rhs[1];

// Tracks which columns of the back buffer have been written since the last swap
static bool column_written[NUM_COLUMNS];
static int columns_written_count = 0;

// Set once back buffer has finished filling
// So buffers can be swapped when rev has finished
static volatile bool frame_ready = false;

bool frame_buffer_back_buffer_free(void) {
    return !frame_ready;
}

void frame_buffer_publish_column(int col, const uint8_t column_levels[LEDS_PER_WING][CHANNELS], int bcm_bits) {
    __dmb(); // memory barrier - keeps cross-core memory accesses in order here

    // Encode this column's levels into column_words, staged into this column's slot of the back frame
    shift_encoder_encode(column_levels, bcm_bits, back_frame_lhs_words[col], back_frame_rhs_words[col]);

    if (!column_written[col]) {
        column_written[col] = true;
        columns_written_count++;

        if (columns_written_count == NUM_COLUMNS) {
            __dmb(); // memory barrier - keeps cross-core memory accesses in order here
            frame_ready = true;
            columns_written_count = 0;
            memset(column_written, 0, sizeof(column_written));
        }
    }
}

bool frame_buffer_swap_pending(void) {
    if (!frame_ready) return false;
    __dmb(); // memory barrier - keeps cross-core memory accesses in order here

    // Swap pointers
    uint16_t (*swap_lhs)[BCM_BITS_MAX][16] = frame_lhs_words; // temp stash pointer to swap
    uint16_t (*swap_rhs)[BCM_BITS_MAX][16] = frame_rhs_words;
    frame_lhs_words = back_frame_lhs_words;
    frame_rhs_words = back_frame_rhs_words;
    back_frame_lhs_words = swap_lhs;
    back_frame_rhs_words = swap_rhs;

    __dmb(); // memory barrier - keeps cross-core memory accesses in order here
    frame_ready = false;
    return true;
}
