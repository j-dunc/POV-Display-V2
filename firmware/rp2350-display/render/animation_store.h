#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "frame_geometry.h"

// Frames currently in flash, 0 if there is no animation to play
uint32_t animation_store_frame_count(void);

// Picks up the animation left in flash by the last completed upload, so it plays again
// Call at startup
void animation_store_init(void);

// Reads one column out of a stored frame, straight from flash through XIP
// Raw uncalibrated 0-255 RGB
// Calibrated and encoded at playback time to save storage cost
void animation_store_read_column(uint32_t frame_index, int col, uint8_t dest[LEDS_PER_WING][CHANNELS]);

// Erases flash region for frame_count
bool animation_store_begin_write(uint32_t frame_count);

// Writes one column to flash - columns land as a plain sequential stream, so must
// be called in order: frame 0's columns 0..NUM_COLUMNS-1, then frame 1's, and so on
void animation_store_write_column(int col, const uint8_t rgb[LEDS_PER_WING][CHANNELS]);

// Marks the animation written since begin_write as ready to play
void animation_store_end_write(void);

// Forgets the stored animation so it stops playing, letting a pushed frame or test
// pattern hold the display. The flash contents are left alone, erased by a new upload
void animation_store_clear(void);
