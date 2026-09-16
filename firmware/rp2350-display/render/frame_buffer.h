#pragma once

#include <stdint.h>

#include "led_map.h"

/*
Frame buffer for each wing is the 48 LEDs in spatial order,
the 3 channels of each, and each channels brightness
*/

// 8 is the hard ceiling since brightness levels are stored as uint8_t (0-255)
#define BCM_BITS_MAX 8

// Precomputes the (shift-cycle, bit-mask) pair for every (led, channel) into an LUT
void frame_buffer_init(void);

void frame_buffer_clear(uint8_t frame[LEDS_PER_WING][CHANNELS]);
void frame_buffer_set(uint8_t frame[LEDS_PER_WING][CHANNELS], int led, int channel, uint8_t level);

/*
Contains a 16-cycle word array per BCM bit-plane, per column. Sized to max BCM bits
per column. Note these are different from LHS to RHS only because bit layout differs
per wing. Column-Frame mapping is identical between LHS and RHS because column
identifies a fixed angular position - difference in wing positions is handled by
the render loop.

Double buffered: this is the front buffer, which render.c reads every revolution.
image_source.c publishes new frames into a separate back buffer via
frame_buffer_publish_column(), which marks it ready when a full frame has landed;
frame_buffer_swap_pending() then swaps it in for render.c to pick up.
*/
extern uint16_t (*image_lhs_words)[BCM_BITS_MAX][16]; // pointer to one column's frame buffer of 16-bit words
extern uint16_t (*image_rhs_words)[BCM_BITS_MAX][16];

// Encodes one column's spatial frame into temporal shift words and stages it in the
// back buffer, marking a full frame ready to swap in once ready
void frame_buffer_publish_column(int col, const uint8_t lhs_frame[LEDS_PER_WING][CHANNELS],
                                  const uint8_t rhs_frame[LEDS_PER_WING][CHANNELS], int bcm_bits);

// Swaps the front/back buffers if a full frame has finished landing in the back
// buffer since the last swap. Called by render.c once per rev
void frame_buffer_swap_pending(void);
