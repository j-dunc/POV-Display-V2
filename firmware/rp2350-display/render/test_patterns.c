#include "test_patterns.h"
#include "image_source.h"
#include "frame_buffer.h"

#include <stdint.h>

// Test pattern: one channel ramps 0 to full scale once per revolution
// Reveals banding/clipping
// Used to tune channel scale and gamma
void test_pattern_build_channel_ramp(int channel) {
    int bcm_bits = image_source_get_bcm_bits();

    // Column content is a function of angle only, not which wing so frame same for each wing
    uint8_t frame[LEDS_PER_WING][CHANNELS];

    for (int col = 0; col < NUM_COLUMNS; col++) {
        frame_buffer_clear(frame);

        uint8_t level_255 = (uint8_t)(col * 255 / (NUM_COLUMNS - 1));

        for (int led = 0; led < LEDS_PER_WING; led++) {
            frame_buffer_set(frame, led, channel, image_source_apply_calibration(level_255, channel, led, col));
        }

        // Encodes column and stages it in the back buffer, swapping it in once the full frame has landed
        frame_buffer_publish_column(col, frame, bcm_bits);
    }
}

// Test pattern: all three channels ramp together, 0 to full scale once per revolution
// Should be black-gray-white gradient revealing white-point errors
// Used to tune channel scales
void test_pattern_build_gray_ramp(void) {
    int bcm_bits = image_source_get_bcm_bits();

    // Column content is a function of angle only, not which wing so frame same for each wing
    uint8_t frame[LEDS_PER_WING][CHANNELS];

    for (int col = 0; col < NUM_COLUMNS; col++) {
        frame_buffer_clear(frame);

        uint8_t level_255 = (uint8_t)(col * 255 / (NUM_COLUMNS - 1));

        for (int led = 0; led < LEDS_PER_WING; led++) {
            for (int ch = 0; ch < CHANNELS; ch++) {
                frame_buffer_set(frame, led, ch, image_source_apply_calibration(level_255, ch, led, col));
            }
        }

        // Encodes column and stages it in the back buffer, swapping it in once the full frame has landed
        frame_buffer_publish_column(col, frame, bcm_bits);
    }
}

// Test pattern: flat full-scale white, same at every column and every LED.
// Isolated radial falloff correction
// Also can be used to find max safe duty fraction before brownout
void test_pattern_build_radial_white(void) {
    int bcm_bits = image_source_get_bcm_bits();

    // Column content same for each column so build once
    uint8_t frame[LEDS_PER_WING][CHANNELS];
    frame_buffer_clear(frame);
    for (int led = 0; led < LEDS_PER_WING; led++) {
        for (int ch = 0; ch < CHANNELS; ch++) {
            frame_buffer_set(frame, led, ch, image_source_apply_calibration(255, ch, led, 0));
        }
    }

    for (int col = 0; col < NUM_COLUMNS; col++) {
        // Encodes column and stages it in the back buffer, swapping it in once the full frame has landed
        frame_buffer_publish_column(col, frame, bcm_bits);
    }
}

// Converts a hue in [0, 360] to full-saturation, full-value RGB at 0-255 scale.
// The hue wheel is divided into 6 sectors of 60 degrees each
// In each sector, one channel is max, one is zero, and one ramps linearly (up or down)
static void hue_to_rgb(float hue_deg, uint8_t rgb[CHANNELS]) {

    // Which 60-degree sector we're in (0-5), and how far through it (0-1)
    int sector = (int)(hue_deg / 60.0f);
    float sector_frac = (hue_deg / 60.0f) - sector;

    // Ramping channel rises in an even sector, and falls in an odd sector
    float ramp_frac = (sector % 2 == 0) ? sector_frac : (1.0f - sector_frac);

    uint8_t ramp = (uint8_t)(255 * ramp_frac + 0.5f); // round rather than truncate

    switch (sector) {
        case 0: rgb[RED] = 255; rgb[GREEN] = ramp; rgb[BLUE] = 0; break; // red -> yellow
        case 1: rgb[RED] = ramp; rgb[GREEN] = 255; rgb[BLUE] = 0; break; // yellow -> green
        case 2: rgb[RED] = 0; rgb[GREEN] = 255; rgb[BLUE] = ramp; break; // green -> cyan
        case 3: rgb[RED] = 0; rgb[GREEN] = ramp; rgb[BLUE] = 255; break; // cyan -> blue
        case 4: rgb[RED] = ramp; rgb[GREEN] = 0; rgb[BLUE] = 255; break; // blue -> magenta
        default: rgb[RED] = 255; rgb[GREEN] = 0; rgb[BLUE] = ramp; break; // magenta -> red
    }
}

// Smooth rainbow test pattern: hue sweeps 0-360 degrees once per revolution
void test_pattern_build_rainbow(void) {
    int bcm_bits = image_source_get_bcm_bits();

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
                frame_buffer_set(frame, led, ch, image_source_apply_calibration(rgb[ch], ch, led, col));
            }
        }

        // Encodes column and stages it in the back buffer, swapping it in once the full frame has landed
        frame_buffer_publish_column(col, frame, bcm_bits);
    }
}
