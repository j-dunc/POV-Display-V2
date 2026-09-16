#include "frame_buffer.h"
#include "pins.h"

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

// Note bit position within word for a given chip differs by wing
void frame_buffer_to_words(const uint8_t lhs_frame[LEDS_PER_WING][CHANNELS],
                            const uint8_t rhs_frame[LEDS_PER_WING][CHANNELS],
                            int plane,
                            uint16_t lhs_words[16], uint16_t rhs_words[16]) {
    // Set both word arrays to 0s so we can set bits with OR
    for (int i = 0; i < 16; i++) {
        lhs_words[i] = 0;
        rhs_words[i] = 0;
    }

    // Find mapped chip and pin for each led and channel
    for (int led = 0; led < LEDS_PER_WING; led++) {
        for (int ch = 0; ch < CHANNELS; ch++) {
            led_coord pos = led_map[led][ch];

            /* First bit shifted ends up on last shift register stage after 16 clocks
            so pin n is set on clock cycle 15 - n */
            int cycle = 15 - pos.pin;

            // Set LHS words
            int bit = LHS_DATA_PINS[pos.chip]; // Bit pos of chip in word
            if ((lhs_frame[led][ch] >> plane) & 1) { // Check if brightness bit for this bitplane is set (shift level right by plane bits to get bit corresponding to bitplane as LSB then mask off other bits)
                lhs_words[cycle] |= (1u << bit); // Use mask to set bit of chip in word of pin if set in frame
            }
            // Set RHS words
            bit = pos.chip; // For RHS, bit n corresponds to chip n
            if ((rhs_frame[led][ch] >> plane) & 1) { // Check if brightness bit for this bitplane is set
                rhs_words[cycle] |= (1u << bit); // Set bit of chip in word of pin if set in frame
            }
        }
    }
}
