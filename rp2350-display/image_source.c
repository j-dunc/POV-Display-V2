#include "image_source.h"
#include "led_map.h"
#include "frame_buffer.h"

// Stores the entire image, word array per BCM bit-plane per column
uint16_t image_lhs_words[NUM_COLUMNS][BCM_BITS][16];
uint16_t image_rhs_words[NUM_COLUMNS][BCM_BITS][16];

// Green LEDs are brighter at same current so derate at buildtime
// Note this can't be done in hardware as RGB are mixed on a single chip / resistor
#define GREEN_SCALE 0.4f

// Fixed palette of full-scale colours, stepped through around the circle
static const uint8_t PALETTE[][CHANNELS] = {
    /*
    {BCM_MAX_LEVEL, 0, 0},             // red
    {0, BCM_MAX_LEVEL, 0},             // green
    {0, 0, BCM_MAX_LEVEL},             // blue
    {BCM_MAX_LEVEL, BCM_MAX_LEVEL, 0}, // yellow
    {0, BCM_MAX_LEVEL, BCM_MAX_LEVEL}, // cyan
    {BCM_MAX_LEVEL, 0, BCM_MAX_LEVEL}, // magenta
    */

    // Red with increasing brightnesses
    {1, 0, 0},  // plane 0 (LSB)
    {2, 0, 0},  // plane 1
    {4, 0, 0},  // plane 2
    {8, 0, 0},  // plane 3
    {16, 0, 0}, // plane 4
    {32, 0, 0}, // plane 5 (MSB)
};
#define PALETTE_SIZE (sizeof(PALETTE) / sizeof(PALETTE[0]))

// Colour-wheel test pattern: steps through the palette in equal-sized bands
void image_source_build(void) {

    // Column content is a function of angle only, not which wing so frame same for each wing
    uint8_t frame[LEDS_PER_WING][CHANNELS];

    for (int col = 0; col < NUM_COLUMNS; col++) {
        frame_buffer_clear(frame);

        int band = col * PALETTE_SIZE / NUM_COLUMNS;

        for (int led = 0; led < LEDS_PER_WING; led++) {
            for (int ch = 0; ch < CHANNELS; ch++) {
                uint8_t level = PALETTE[band][ch];
                if (ch == GREEN) level = (uint8_t)(level * GREEN_SCALE);
                frame_buffer_set(frame, led, ch, level);
            }
        }

        // Encodes frame to different bit layouts for each wing for shifting, one per BCM bit-plane
        for (int plane = 0; plane < BCM_BITS; plane++) {
            frame_buffer_to_words(frame, frame, plane, image_lhs_words[col][plane], image_rhs_words[col][plane]);
        }
    }
}
