#include "image_source.h"
#include "led_map.h"
#include "frame_buffer.h"

#include <math.h>
#include <string.h>
#include <stdbool.h>

// Colour calibration
// per channel linear scale
// shared gamma exponent
// radial exponent (corrects for LEDs near centre appearing brighter)
// BCM bit depth (1-8)
// tuned via SPI with CMD_SET_CALIBRATION
typedef struct {
    float scale[CHANNELS];
    float gamma;
    float radial_gamma;
    bool dither_enabled;
    int bcm_bits;
} color_calibration_t;

// This is single source of truth for defaults - values revert to this on reset
static color_calibration_t calibration = {
    .scale = { 1.0f, 0.5f, 0.3f }, // R, G, B
    .gamma = 3.5f,
    .radial_gamma = 0.6f, // 0 = no radial correction
    .dither_enabled = true,
    .bcm_bits = 8,
};

// 8x8 ordered (Bayer) dither matrix, values 0-63
// Indexed by LED/column position
// Spreads rounding errors evenly
static const uint8_t BAYER_8X8[8][8] = {
    { 0, 32,  8, 40,  2, 34, 10, 42},
    {48, 16, 56, 24, 50, 18, 58, 26},
    {12, 44,  4, 36, 14, 46,  6, 38},
    {60, 28, 52, 20, 62, 30, 54, 22},
    { 3, 35, 11, 43,  1, 33,  9, 41},
    {51, 19, 59, 27, 49, 17, 57, 25},
    {15, 47,  7, 39, 13, 45,  5, 37},
    {63, 31, 55, 23, 61, 29, 53, 21},
};

// Converts one raw 0-255 channel value, at a given radial LED position and
// column, to a 0-bcm_max_level level - applying gamma, channel, and radial balance
// and bayer dithering
static uint8_t apply_calibration(uint8_t raw_255, int channel, int led, int col) {
    // Apply gamma and channel scale
    float normalized = raw_255 / 255.0f;
    float corrected = powf(normalized, calibration.gamma) * calibration.scale[channel]; // Cout = Cin^Gamma

    // Apply radial gamma
    // Use LED + 1 so the inner LED doesn't turn off - otherwise it would 0^anything = 0
    float radial_norm = (float)(led + 1) / LEDS_PER_WING; // > 0 at centre, 1 at tip
    corrected *= powf(radial_norm, calibration.radial_gamma);

    int bcm_max_level = (1 << calibration.bcm_bits) - 1;

    // Ordered dithering:
    // Instead of always rounding to nearest int, rounding threshold varies
    // per LED (given by matrix) preventing banding
    float level_f = corrected * bcm_max_level;
    int base = (int)level_f; // truncate
    float frac = level_f - base;
    int level;
    if (calibration.dither_enabled) {
        float threshold = BAYER_8X8[led % 8][col % 8] / 64.0f;
        level = (frac > threshold) ? base + 1 : base;
    } else {
        level = (frac >= 0.5f) ? base + 1 : base; // plain round-to-nearest
    }
    if (level > bcm_max_level) level = bcm_max_level; // clamp to max level

    return (uint8_t)level;
}

// Raw (uncalibrated) copy of the last frame uploaded via image_source_set_column
// Kept so calibration changes are applied to raw frame
static uint8_t last_frame[NUM_COLUMNS][LEDS_PER_WING][CHANNELS];

// Tracks what's actually currently on screen
// So calibration changes know what to rebuild
typedef enum {
    IMAGE_SOURCE_NONE, // still blank - nothing built yet
    IMAGE_SOURCE_UPLOADED_FRAME,
    IMAGE_SOURCE_TEST_PATTERN,
} image_source_mode_t;

static image_source_mode_t current_mode = IMAGE_SOURCE_NONE;
static uint8_t current_test_pattern_id = 0;

void image_source_set_calibration(float r_scale, float g_scale, float b_scale, float gamma, float radial_gamma, bool dither_enabled, int bcm_bits) {
    calibration.scale[RED] = r_scale;
    calibration.scale[GREEN] = g_scale;
    calibration.scale[BLUE] = b_scale;
    calibration.gamma = gamma;
    calibration.radial_gamma = radial_gamma;
    calibration.dither_enabled = dither_enabled;
    if (bcm_bits < 1) bcm_bits = 1; // clamp
    if (bcm_bits > BCM_BITS_MAX) bcm_bits = BCM_BITS_MAX;
    calibration.bcm_bits = bcm_bits;

    // Reapply to whatever is actually currently on screen
    switch (current_mode) {
        case IMAGE_SOURCE_UPLOADED_FRAME:
            for (int col = 0; col < NUM_COLUMNS; col++) {
                image_source_set_column(col, last_frame[col]);
            }
            break;
        case IMAGE_SOURCE_TEST_PATTERN:
            image_source_show_test_pattern(current_test_pattern_id);
            break;
        case IMAGE_SOURCE_NONE:
            break; // still blank
    }
}

void image_source_get_calibration(calibration_payload_t *out) {
    out->r_scale = calibration.scale[RED];
    out->g_scale = calibration.scale[GREEN];
    out->b_scale = calibration.scale[BLUE];
    out->gamma = calibration.gamma;
    out->radial_gamma = calibration.radial_gamma;
    out->dither_enabled = calibration.dither_enabled ? 1.0f : 0.0f;
    out->bcm_bits = (float)calibration.bcm_bits;
}

int image_source_get_bcm_bits(void) {
    return calibration.bcm_bits;
}

// Stores the entire image, word array per BCM bit-plane per column. Sized to BCM_BITS_MAX
// only planes 0..bcm_bits-1 are meaningful at any time
uint16_t image_lhs_words[NUM_COLUMNS][BCM_BITS_MAX][16];
uint16_t image_rhs_words[NUM_COLUMNS][BCM_BITS_MAX][16];


// Test pattern: one channel ramps 0 to full scale once per revolution
// Reveals banding/clipping
// Used to tune channel scale and gamma
void image_source_build_channel_ramp(int channel) {

    // Column content is a function of angle only, not which wing so frame same for each wing
    uint8_t frame[LEDS_PER_WING][CHANNELS];

    for (int col = 0; col < NUM_COLUMNS; col++) {
        frame_buffer_clear(frame);

        uint8_t level_255 = (uint8_t)(col * 255 / (NUM_COLUMNS - 1));

        for (int led = 0; led < LEDS_PER_WING; led++) {
            frame_buffer_set(frame, led, channel, apply_calibration(level_255, channel, led, col));
        }

        // Encodes frame to different bit layouts for each wing for shifting, one per BCM bit-plane
        for (int plane = 0; plane < calibration.bcm_bits; plane++) {
            frame_buffer_to_words(frame, frame, plane, image_lhs_words[col][plane], image_rhs_words[col][plane]);
        }
    }
}

// Test pattern: all three channels ramp together, 0 to full scale once per revolution
// Should be black-gray-white gradient revealing white-point errors
// Used to tune channel scales
void image_source_build_gray_ramp(void) {

    // Column content is a function of angle only, not which wing so frame same for each wing
    uint8_t frame[LEDS_PER_WING][CHANNELS];

    for (int col = 0; col < NUM_COLUMNS; col++) {
        frame_buffer_clear(frame);

        uint8_t level_255 = (uint8_t)(col * 255 / (NUM_COLUMNS - 1));

        for (int led = 0; led < LEDS_PER_WING; led++) {
            for (int ch = 0; ch < CHANNELS; ch++) {
                frame_buffer_set(frame, led, ch, apply_calibration(level_255, ch, led, col));
            }
        }

        // Encodes frame to different bit layouts for each wing for shifting, one per BCM bit-plane
        for (int plane = 0; plane < calibration.bcm_bits; plane++) {
            frame_buffer_to_words(frame, frame, plane, image_lhs_words[col][plane], image_rhs_words[col][plane]);
        }
    }
}

// Test pattern: flat full-scale white, same at every column and every LED.
// Isolated radial falloff correction
// Also can be used to find max safe duty fraction before brownout
void image_source_build_radial_white(void) {
    // Column content same for each column so build once
    uint8_t frame[LEDS_PER_WING][CHANNELS];
    frame_buffer_clear(frame);
    for (int led = 0; led < LEDS_PER_WING; led++) {
        for (int ch = 0; ch < CHANNELS; ch++) {
            frame_buffer_set(frame, led, ch, apply_calibration(255, ch, led, 0));
        }
    }

    for (int col = 0; col < NUM_COLUMNS; col++) {
        // Encodes frame to different bit layouts for each wing for shifting, one per BCM bit-plane
        for (int plane = 0; plane < calibration.bcm_bits; plane++) {
            frame_buffer_to_words(frame, frame, plane, image_lhs_words[col][plane], image_rhs_words[col][plane]);
        }
    }
}

// Converts a hue in [0, 360) to full-saturation, full-value RGB at 0-255 scale.
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
                frame_buffer_set(frame, led, ch, apply_calibration(rgb[ch], ch, led, col));
            }
        }

        // Encodes frame to different bit layouts for each wing for shifting, one per BCM bit-plane
        for (int plane = 0; plane < calibration.bcm_bits; plane++) {
            frame_buffer_to_words(frame, frame, plane, image_lhs_words[col][plane], image_rhs_words[col][plane]);
        }
    }
}

// Calls the build_* function for one TEST_PATTERN_* id, and records
// it as the current display source so a later calibration change rebuilds it
void image_source_show_test_pattern(uint8_t pattern_id) {
    switch (pattern_id) {
        case TEST_PATTERN_RED_RAMP:     image_source_build_channel_ramp(RED); break;
        case TEST_PATTERN_GREEN_RAMP:   image_source_build_channel_ramp(GREEN); break;
        case TEST_PATTERN_BLUE_RAMP:    image_source_build_channel_ramp(BLUE); break;
        case TEST_PATTERN_GRAY_RAMP:    image_source_build_gray_ramp(); break;
        case TEST_PATTERN_RADIAL_WHITE: image_source_build_radial_white(); break;
        case TEST_PATTERN_RAINBOW:      image_source_build_rainbow(); break;
        default: return; // unknown pattern id, ignore - leave current_mode untouched
    }
    current_mode = IMAGE_SOURCE_TEST_PATTERN;
    current_test_pattern_id = pattern_id;
}

// Sets one column from raw 0-255 RGB, e.g. pushed live over SPI
void image_source_set_column(int col, const uint8_t rgb[LEDS_PER_WING][CHANNELS]) {
    // Remember the raw (uncalibrated) values so a later calibration change applies to raw frame
    memmove(last_frame[col], rgb, sizeof(last_frame[col])); // Not memcpy as source and destination may be same if called from set_calibration
    current_mode = IMAGE_SOURCE_UPLOADED_FRAME;

    uint8_t frame[LEDS_PER_WING][CHANNELS];
    frame_buffer_clear(frame);

    for (int led = 0; led < LEDS_PER_WING; led++) {
        for (int ch = 0; ch < CHANNELS; ch++) {
            frame_buffer_set(frame, led, ch, apply_calibration(rgb[led][ch], ch, led, col));
        }
    }

    // Encodes frame to different bit layouts for each wing for shifting, one per BCM bit-plane
    for (int plane = 0; plane < calibration.bcm_bits; plane++) {
        frame_buffer_to_words(frame, frame, plane, image_lhs_words[col][plane], image_rhs_words[col][plane]);
    }
}
