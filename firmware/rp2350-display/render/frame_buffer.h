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


// Transforms spatial frame buffers to temporal word arrays for shifting out, for every BCM bit-plane (0 = LSB .. bcm_bits-1 = MSB)
void frame_buffer_to_planes(const uint8_t lhs_frame[LEDS_PER_WING][CHANNELS],
                             const uint8_t rhs_frame[LEDS_PER_WING][CHANNELS],
                             int bcm_bits,
                             uint16_t lhs_words[][16], uint16_t rhs_words[][16]);
