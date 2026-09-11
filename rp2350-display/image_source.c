#include "image_source.h"
#include "led_map.h"
#include "frame_buffer.h"

#include <math.h>

// Stores the entire image, word array per BCM bit-plane per column
uint16_t image_lhs_words[NUM_COLUMNS][BCM_BITS][16];
uint16_t image_rhs_words[NUM_COLUMNS][BCM_BITS][16];


void image_source_build_ramp(void) {

    // Column content is a function of angle only, not which wing so frame same for each wing
    uint8_t frame[LEDS_PER_WING][CHANNELS];

    for (int col = 0; col < NUM_COLUMNS; col++) {
        frame_buffer_clear(frame);

        uint8_t level = (uint8_t)(col * BCM_MAX_LEVEL / (NUM_COLUMNS - 1));

        for (int led = 0; led < LEDS_PER_WING; led++) {
            frame_buffer_set(frame, led, RED, level);
        }

        // Encodes frame to different bit layouts for each wing for shifting, one per BCM bit-plane
        for (int plane = 0; plane < BCM_BITS; plane++) {
            frame_buffer_to_words(frame, frame, plane, image_lhs_words[col][plane], image_rhs_words[col][plane]);
        }
    }
}

// Green LEDs are brighter at same current so derate at buildtime
// Note this can't be done in hardware as RGB are mixed on a single chip / resistor
#define GREEN_SCALE 0.3f

// Fixed palette of full-scale colours, stepped through around the circle
static const uint8_t PALETTE[][CHANNELS] = {
    {BCM_MAX_LEVEL, 0, 0},             // red
    {0, BCM_MAX_LEVEL, 0},             // green
    {0, 0, BCM_MAX_LEVEL},             // blue
    {BCM_MAX_LEVEL, BCM_MAX_LEVEL, 0}, // yellow
    {0, BCM_MAX_LEVEL, BCM_MAX_LEVEL}, // cyan
    {BCM_MAX_LEVEL, 0, BCM_MAX_LEVEL}, // magenta
};
#define PALETTE_SIZE (sizeof(PALETTE) / sizeof(PALETTE[0]))

// Colour-wheel test pattern: steps through PALETTE in equal-sized bands around the circle
void image_source_build_palette(void) {

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

// Converts a hue in [0, 360) to full-saturation, full-value RGB at BCM_MAX_LEVEL scale.
// The hue wheel is divided into 6 sectors of 60 degrees each
// In each sector, one channel is max, one is zero, and one ramps linearly (up or down)
static void hue_to_rgb(float hue_deg, uint8_t rgb[CHANNELS]) {

    // Which 60-degree sector we're in (0-5), and how far through it (0-1)
    int sector = (int)(hue_deg / 60.0f);
    float sector_frac = (hue_deg / 60.0f) - sector;

    // Ramping channel rises in an even sector, and falls in an odd sector
    float ramp_frac = (sector % 2 == 0) ? sector_frac : (1.0f - sector_frac);

    uint8_t ramp = (uint8_t)(BCM_MAX_LEVEL * ramp_frac);

    switch (sector) {
        case 0: rgb[RED] = BCM_MAX_LEVEL; rgb[GREEN] = ramp; rgb[BLUE] = 0; break; // red -> yellow
        case 1: rgb[RED] = ramp; rgb[GREEN] = BCM_MAX_LEVEL; rgb[BLUE] = 0; break; // yellow -> green
        case 2: rgb[RED] = 0; rgb[GREEN] = BCM_MAX_LEVEL; rgb[BLUE] = ramp; break; // green -> cyan
        case 3: rgb[RED] = 0; rgb[GREEN] = ramp; rgb[BLUE] = BCM_MAX_LEVEL; break; // cyan -> blue
        case 4: rgb[RED] = ramp; rgb[GREEN] = 0; rgb[BLUE] = BCM_MAX_LEVEL; break; // blue -> magenta
        default: rgb[RED] = BCM_MAX_LEVEL; rgb[GREEN] = 0; rgb[BLUE] = ramp; break; // magenta -> red
    }
}

// Smooth rainbow test pattern: hue sweeps 0-360 degrees once per revolution
void image_source_build_rainbow(void) {

    // Column content is a function of angle only, not which wing so frame same for each wing
    uint8_t frame[LEDS_PER_WING][CHANNELS];

    // Draw each column at the determined hsv converted to rgb
    for (int col = 0; col < NUM_COLUMNS; col++) {
        frame_buffer_clear(frame);

        float hue = 360.0f * col / NUM_COLUMNS;
        uint8_t rgb[CHANNELS];
        hue_to_rgb(hue, rgb);

        for (int led = 0; led < LEDS_PER_WING; led++) {
            for (int ch = 0; ch < CHANNELS; ch++) {
                uint8_t level = rgb[ch];
                if (ch == GREEN) level = (uint8_t)(level * GREEN_SCALE); // Scale green to match brightnesses
                frame_buffer_set(frame, led, ch, level);
            }
        }

        // Encodes frame to different bit layouts for each wing for shifting, one per BCM bit-plane
        for (int plane = 0; plane < BCM_BITS; plane++) {
            frame_buffer_to_words(frame, frame, plane, image_lhs_words[col][plane], image_rhs_words[col][plane]);
        }
    }
}
