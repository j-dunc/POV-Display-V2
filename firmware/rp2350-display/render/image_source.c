#include "image_source.h"
#include "frame_buffer.h"
#include "test_patterns.h"

#include <math.h>
#include <string.h>
#include <stdbool.h>

// Colour calibration
// per channel linear scale
// shared gamma exponent
// radial exponent (corrects for LEDs near centre appearing brighter)
// max duty fraction (brightness) - limits current to avoid brownout
// BCM bit depth (1-8)
// tuned via SPI with CMD_SET_CALIBRATION
typedef struct {
    float scale[CHANNELS];
    float gamma;
    float radial_gamma;
    float max_duty_fraction;
    bool dither_enabled;
    int bcm_bits;
} color_calibration_t;

// This is single source of truth for defaults - values revert to this on reset
static color_calibration_t calibration = {
    .scale = { 1.5f, 0.75f, 0.45f }, // R, G, B
    .gamma = 3.5f,
    .radial_gamma = 0.3f, // 0 = no radial correction
    .max_duty_fraction = 0.2f,
    .dither_enabled = true,
    .bcm_bits = 8,
};

// 16x16 ordered (Bayer) dither matrix, values 0-255
// Indexed by LED/column position
// Spreads rounding errors evenly
static const uint8_t BAYER_16X16[16][16] = {
    {  0,128, 32,160,  8,136, 40,168,  2,130, 34,162, 10,138, 42,170},
    {192, 64,224, 96,200, 72,232,104,194, 66,226, 98,202, 74,234,106},
    { 48,176, 16,144, 56,184, 24,152, 50,178, 18,146, 58,186, 26,154},
    {240,112,208, 80,248,120,216, 88,242,114,210, 82,250,122,218, 90},
    { 12,140, 44,172,  4,132, 36,164, 14,142, 46,174,  6,134, 38,166},
    {204, 76,236,108,196, 68,228,100,206, 78,238,110,198, 70,230,102},
    { 60,188, 28,156, 52,180, 20,148, 62,190, 30,158, 54,182, 22,150},
    {252,124,220, 92,244,116,212, 84,254,126,222, 94,246,118,214, 86},
    {  3,131, 35,163, 11,139, 43,171,  1,129, 33,161,  9,137, 41,169},
    {195, 67,227, 99,203, 75,235,107,193, 65,225, 97,201, 73,233,105},
    { 51,179, 19,147, 59,187, 27,155, 49,177, 17,145, 57,185, 25,153},
    {243,115,211, 83,251,123,219, 91,241,113,209, 81,249,121,217, 89},
    { 15,143, 47,175,  7,135, 39,167, 13,141, 45,173,  5,133, 37,165},
    {207, 79,239,111,199, 71,231,103,205, 77,237,109,197, 69,229,101},
    { 63,191, 31,159, 55,183, 23,151, 61,189, 29,157, 53,181, 21,149},
    {255,127,223, 95,247,119,215, 87,253,125,221, 93,245,117,213, 85},
};

/*
Pre-calculated calibration look-up tables rather than calculating per pixel
  corrected = powf(raw/255, gamma) * scale[ch] * powf((led+1)/LEDS_PER_WING, radial_gamma)
              \__________________/   \_____________________________________________/
                 gamma_lut[raw]                    channel_gain[ch][led]
*/
static float gamma_lut[256];
static float channel_gain[CHANNELS][LEDS_PER_WING];

void rebuild_calibration(void) {
    for (int raw_255 = 0; raw_255 < 256; raw_255++) {
        gamma_lut[raw_255] = powf(raw_255 / 255.0f, calibration.gamma); // Cout = Cin^Gamma
    }

    int bcm_max_level = (1 << calibration.bcm_bits) - 1;
    for (int led = 0; led < LEDS_PER_WING; led++) {
        // Use LED + 1 so the inner LED doesn't turn off - otherwise it would 0^anything = 0
        float radial_norm = (float)(led + 1) / LEDS_PER_WING; // > 0 at centre, 1 at tip
        // Doesn't depend on channel, so compute once per led rather than once per (ch, led)
        float radial_factor = powf(radial_norm, calibration.radial_gamma) * bcm_max_level;

        for (int ch = 0; ch < CHANNELS; ch++) {
            channel_gain[ch][led] = calibration.scale[ch] * radial_factor;
        }
    }
}

// Converts one raw 0-255 channel value, at a given radial LED position and
// column, to a 0-bcm_max_level level - applying gamma, channel, and radial balance
// and bayer dithering
uint8_t image_source_apply_calibration(uint8_t raw_255, int channel, int led, int col) {
    // Gamma, channel scale and radial gamma are all accounted for in these two lookups
    float level_f = gamma_lut[raw_255] * channel_gain[channel][led];

    int bcm_max_level = (1 << calibration.bcm_bits) - 1;

    // Ordered dithering:
    // Instead of always rounding to nearest int, rounding threshold varies
    // per LED (given by matrix) preventing banding
    int base = (int)level_f; // truncate
    float frac = level_f - base;
    int level;
    if (calibration.dither_enabled) {
        float threshold = BAYER_16X16[led % 16][col % 16] / 256.0f;
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

void image_source_set_calibration(float r_scale, float g_scale, float b_scale, float gamma, float radial_gamma, float max_duty_fraction, bool dither_enabled, int bcm_bits) {
    calibration.scale[RED] = r_scale;
    calibration.scale[GREEN] = g_scale;
    calibration.scale[BLUE] = b_scale;
    calibration.gamma = gamma;
    calibration.radial_gamma = radial_gamma;
    calibration.max_duty_fraction = max_duty_fraction;
    calibration.dither_enabled = dither_enabled;
    if (bcm_bits < 1) bcm_bits = 1; // clamp
    if (bcm_bits > BCM_BITS_MAX) bcm_bits = BCM_BITS_MAX;
    calibration.bcm_bits = bcm_bits;

    rebuild_calibration();

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
    out->max_duty_fraction = calibration.max_duty_fraction;
    out->dither_enabled = calibration.dither_enabled ? 1.0f : 0.0f;
    out->bcm_bits = (float)calibration.bcm_bits;
}

int image_source_get_bcm_bits(void) {
    return calibration.bcm_bits;
}

float image_source_get_max_duty_fraction(void) {
    return calibration.max_duty_fraction;
}

// Calls the test_pattern_build_* function for one TEST_PATTERN_* id, and records
// it as the current display source so a later calibration change rebuilds it
void image_source_show_test_pattern(uint8_t pattern_id) {
    switch (pattern_id) {
        case TEST_PATTERN_RED_RAMP:     test_pattern_build_channel_ramp(RED); break;
        case TEST_PATTERN_GREEN_RAMP:   test_pattern_build_channel_ramp(GREEN); break;
        case TEST_PATTERN_BLUE_RAMP:    test_pattern_build_channel_ramp(BLUE); break;
        case TEST_PATTERN_GRAY_RAMP:    test_pattern_build_gray_ramp(); break;
        case TEST_PATTERN_RADIAL_WHITE: test_pattern_build_radial_white(); break;
        case TEST_PATTERN_RAINBOW:      test_pattern_build_rainbow(); break;
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
            frame_buffer_set(frame, led, ch, image_source_apply_calibration(rgb[led][ch], ch, led, col));
        }
    }

    // Encodes column and stages it in the back buffer, swapping it in once the full frame has landed
    frame_buffer_publish_column(col, frame, frame, calibration.bcm_bits);
}
