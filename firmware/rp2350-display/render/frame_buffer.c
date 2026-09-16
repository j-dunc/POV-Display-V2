#include "frame_buffer.h"
#include "pins.h"

// Precomputed per (led, channel): which shift cycle its bit lands in, and its bit
// mask within that cycle's word for each wing. Bit position within word for a given
// chip differs by wing, so lhs_mask and rhs_mask are tracked separately.
typedef struct {
    uint8_t cycle;
    uint16_t lhs_mask;
    uint16_t rhs_mask;
} shift_position_t;

static shift_position_t shift_positions[LEDS_PER_WING][CHANNELS]; // Lookup table

void frame_buffer_init(void) {
    for (int led = 0; led < LEDS_PER_WING; led++) {
        for (int ch = 0; ch < CHANNELS; ch++) {
            led_coord pos = led_map[led][ch];

            /* First bit shifted ends up on last shift register stage after 16 clocks
            so pin n is set on clock cycle 15 - n */
            shift_positions[led][ch].cycle = (uint8_t)(15 - pos.pin);
            shift_positions[led][ch].lhs_mask = (uint16_t)(1u << LHS_DATA_PINS[pos.chip]); // Bit pos of chip in word
            shift_positions[led][ch].rhs_mask = (uint16_t)(1u << pos.chip); // For RHS, bit n corresponds to chip n
        }
    }
}

// Loop through leds and channels and set all to off
void frame_buffer_clear(uint8_t frame[LEDS_PER_WING][CHANNELS]) {
    for (int led = 0; led < LEDS_PER_WING; led++)
        for (int ch = 0; ch < CHANNELS; ch++)
            frame[led][ch] = 0;
}

// Set a specific led and channel to a brightness level (0 to the active bcm_max_level)
void frame_buffer_set(uint8_t frame[LEDS_PER_WING][CHANNELS], int led, int channel, uint8_t level) {
    frame[led][channel] = level;
}

void frame_buffer_to_planes(const uint8_t lhs_frame[LEDS_PER_WING][CHANNELS],
                             const uint8_t rhs_frame[LEDS_PER_WING][CHANNELS],
                             int bcm_bits,
                             uint16_t lhs_words[][16], uint16_t rhs_words[][16]) {
    // Set all planes word arrays to 0s so we can set bits with OR
    for (int plane = 0; plane < bcm_bits; plane++) {
        for (int i = 0; i < 16; i++) {
            lhs_words[plane][i] = 0;
            rhs_words[plane][i] = 0;
        }
    }

    // Look up each channel/LEDs cycle and mask
    for (int led = 0; led < LEDS_PER_WING; led++) {
        for (int ch = 0; ch < CHANNELS; ch++) {
            const shift_position_t *entry = &shift_positions[led][ch];
            uint8_t lhs_level = lhs_frame[led][ch];
            uint8_t rhs_level = rhs_frame[led][ch];
            
            // Set words for each plane
            for (int plane = 0; plane < bcm_bits; plane++) {
                if ((lhs_level >> plane) & 1) { // Check if brightness bit for this bitplane is set
                    lhs_words[plane][entry->cycle] |= entry->lhs_mask; // Use mask to set bit
                }
                if ((rhs_level >> plane) & 1) {
                    rhs_words[plane][entry->cycle] |= entry->rhs_mask;
                }
            }
        }
    }
}
