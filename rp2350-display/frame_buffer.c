#include "frame_buffer.h"
#include "pins.h"

// Loop through leds and channels and set all to off
void frame_buffer_clear(bool frame[LEDS_PER_WING][CHANNELS]) {
    for (int led = 0; led < LEDS_PER_WING; led++)
        for (int ch = 0; ch < CHANNELS; ch++)
            frame[led][ch] = false;
}

// Set a specific led and channel on or off
void frame_buffer_set(bool frame[LEDS_PER_WING][CHANNELS], int led, int channel, bool on) {
    frame[led][channel] = on;
}

// Note bit position within word for a given chip differs by wing
void frame_buffer_to_words(const bool lhs_frame[LEDS_PER_WING][CHANNELS],
                            const bool rhs_frame[LEDS_PER_WING][CHANNELS],
                            uint32_t lhs_words[16], uint32_t rhs_words[16]) {
    // Set both word arrays to 0s so we can set bits with OR
    for (int i = 0; i < 16; i++) {
        lhs_words[i] = 0;
        rhs_words[i] = 0;
    }

    // Find mapped chip and pin for each led and channel
    for (int led = 0; led < LEDS_PER_WING; led++) {
        for (int ch = 0; ch < CHANNELS; ch++) {
            led_coord pos = led_map[led][ch];

            // Set LHS words
            int bit = LHS_DATA_PINS[pos.chip]; // Bit pos of chip in word
            if (lhs_frame[led][ch]) {
                lhs_words[pos.pin] |= (1u << bit); // Use mask to set bit of chip in word of pin if set in frame
            }
            // Set RHS words
            bit = pos.chip; // For RHS, bit n corresponds to chip n
            if (rhs_frame[led][ch]) {
                rhs_words[pos.pin] |= (1u << bit); // Set bit of chip in word of pin if set in frame
            }
        }
    }
}
