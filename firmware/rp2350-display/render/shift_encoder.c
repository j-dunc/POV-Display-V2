// shift_encoder: converts one column's spatial brightness levels into the
// temporal shift-out words for both wings, given the LED-to-chip wiring

#include "shift_encoder.h"
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

void shift_encoder_init(void) {
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

void shift_encoder_encode(const uint8_t column_levels[LEDS_PER_WING][CHANNELS], int bcm_bits,
                           uint16_t lhs_column_words[][16], uint16_t rhs_column_words[][16]) {
    // Zero every plane_words array so we can set bits with OR
    for (int plane = 0; plane < bcm_bits; plane++) {
        for (int i = 0; i < 16; i++) {
            lhs_column_words[plane][i] = 0;
            rhs_column_words[plane][i] = 0;
        }
    }

    // Look up each channel/LEDs cycle and mask
    for (int led = 0; led < LEDS_PER_WING; led++) {
        for (int ch = 0; ch < CHANNELS; ch++) {
            const shift_position_t *entry = &shift_positions[led][ch];
            uint8_t level = column_levels[led][ch];

            // Set the bit in this (led, ch)'s cycle position, in each plane's plane_words, where level has that bit set
            for (int plane = 0; plane < bcm_bits; plane++) {
                if ((level >> plane) & 1) { // Check if brightness bit for this bitplane is set
                    lhs_column_words[plane][entry->cycle] |= entry->lhs_mask;
                    rhs_column_words[plane][entry->cycle] |= entry->rhs_mask;
                }
            }
        }
    }
}
