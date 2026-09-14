#include "test_pattern.h"

// Solid colour test pattern
void test_pattern_build(uint8_t frame[NUM_COLUMNS][LEDS_PER_WING][CHANNELS], uint8_t rgb[CHANNELS]) {
    for (int col = 0; col < NUM_COLUMNS; col++) {
        for (int led = 0; led < LEDS_PER_WING; led++) {
            for (int ch = 0; ch < CHANNELS; ch++) {
                frame[col][led][ch] = rgb[ch];
            }
        }
    }
}
