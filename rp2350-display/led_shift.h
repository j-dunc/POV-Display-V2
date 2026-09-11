#pragma once

#include <stdint.h>

// Initialises LHS/RHS PIO state machines and their feeder DMA channels
void led_shift_init(void);

/*
Shifts 16 cycles (one full 16-channel load) into the 9 chips of each wing in parallel
at 25MHz, blocks until complete.
Does not latch so can run when other data is displayed

Each word contains the bit shifted into each shift register in that cycle
LHS: bit n of a word is shifted into GPIO n, note this is not necessarily chip n
RHS: bit n of a word is shifted into GPIO 21+n, corresponding to chip n
Note any bits in the word past the ones used for each chip are nonsense
The array lhs_words contains the 16 words to fill all 16 channels of each chip on a wing during 16 cycles
*/
void led_shift_load(const uint16_t lhs_words[16], const uint16_t rhs_words[16]);

// Pulses the shared LE line once, transferring the most recently loaded
// data from the chips' shift registers into their output latches. Blocks until complete.
void led_shift_latch(void);
