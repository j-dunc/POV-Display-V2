#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "led_map.h"

/*
Frame buffer for each wing is the 48 LEDs in spatial order,
the 3 channels of each, and whether each channel is on
*/

void frame_buffer_clear(bool frame[LEDS_PER_WING][CHANNELS]);
void frame_buffer_set(bool frame[LEDS_PER_WING][CHANNELS], int led, int channel, bool on);

// Transforms spatial frame buffers to temporal word arrays for shifting out
void frame_buffer_to_words(const bool lhs_frame[LEDS_PER_WING][CHANNELS],
                            const bool rhs_frame[LEDS_PER_WING][CHANNELS],
                            uint32_t lhs_words[16], uint32_t rhs_words[16]);
