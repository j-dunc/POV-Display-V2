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
// tuned via SPI with CMD_SET_CALIBRATION
typedef struct {
    float scale[CHANNELS];
    float gamma;
    float radial_gamma;
} color_calibration_t;

static color_calibration_t calibration = {
    .scale = { 1.0f, 0.4f, 0.25f }, // R, G, B
    .gamma = 2.2f,
    .radial_gamma = 0.0f, // 0 = no radial correction
};

// Converts one raw 0-255 channel value, at a given radial LED position, to a
// 0-BCM_MAX_LEVEL level - applying gamma, channel, and radial balance
static uint8_t apply_calibration(uint8_t raw_255, int channel, int led) {
    // Apply gamma and channel scale
    float normalized = raw_255 / 255.0f;
    float corrected = powf(normalized, calibration.gamma) * calibration.scale[channel]; // Cout = Cin^Gamma

    // Apply radial gamma
    // Use LED + 1 so the inner LED doesn't turn off - otherwise it would 0^anything = 0
    float radial_norm = (float)(led + 1) / LEDS_PER_WING; // > 0 at centre, 1 at tip
    corrected *= powf(radial_norm, calibration.radial_gamma);

    // Convert to BCM level
    if (corrected > 1.0f) corrected = 1.0f; // clamp full scale at 1
    return (uint8_t)(corrected * BCM_MAX_LEVEL + 0.5f); // round rather than truncate
}

// Raw (uncalibrated) copy of the last frame uploaded via image_source_set_column
// Kept so calibration changes are applied to raw frame
static uint8_t last_frame[NUM_COLUMNS][LEDS_PER_WING][CHANNELS];
static bool has_uploaded_frame = false;

void image_source_set_calibration(float r_scale, float g_scale, float b_scale, float gamma, float radial_gamma) {
    calibration.scale[RED] = r_scale;
    calibration.scale[GREEN] = g_scale;
    calibration.scale[BLUE] = b_scale;
    calibration.gamma = gamma;
    calibration.radial_gamma = radial_gamma;

    // Reapply to whatever image is currently on screen
    if (has_uploaded_frame) {
        for (int col = 0; col < NUM_COLUMNS; col++) {
            image_source_set_column(col, last_frame[col]);
        }
    }
}

void image_source_get_calibration(calibration_payload_t *out) {
    out->r_scale = calibration.scale[RED];
    out->g_scale = calibration.scale[GREEN];
    out->b_scale = calibration.scale[BLUE];
    out->gamma = calibration.gamma;
    out->radial_gamma = calibration.radial_gamma;
}

// Stores the entire image, word array per BCM bit-plane per column
uint16_t image_lhs_words[NUM_COLUMNS][BCM_BITS][16];
uint16_t image_rhs_words[NUM_COLUMNS][BCM_BITS][16];


void image_source_build_ramp(void) {

    // Column content is a function of angle only, not which wing so frame same for each wing
    uint8_t frame[LEDS_PER_WING][CHANNELS];

    for (int col = 0; col < NUM_COLUMNS; col++) {
        frame_buffer_clear(frame);

        uint8_t level_255 = (uint8_t)(col * 255 / (NUM_COLUMNS - 1));

        for (int led = 0; led < LEDS_PER_WING; led++) {
            frame_buffer_set(frame, led, RED, apply_calibration(level_255, RED, led));
        }

        // Encodes frame to different bit layouts for each wing for shifting, one per BCM bit-plane
        for (int plane = 0; plane < BCM_BITS; plane++) {
            frame_buffer_to_words(frame, frame, plane, image_lhs_words[col][plane], image_rhs_words[col][plane]);
        }
    }
}

// Fixed palette of full-scale colours (0-255), stepped through around the circle
static const uint8_t PALETTE[][CHANNELS] = {
    {255, 0, 0},     // red
    {0, 255, 0},     // green
    {0, 0, 255},     // blue
    {255, 255, 0},   // yellow
    {0, 255, 255},   // cyan
    {255, 0, 255},   // magenta
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
                frame_buffer_set(frame, led, ch, apply_calibration(PALETTE[band][ch], ch, led));
            }
        }

        // Encodes frame to different bit layouts for each wing for shifting, one per BCM bit-plane
        for (int plane = 0; plane < BCM_BITS; plane++) {
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
                frame_buffer_set(frame, led, ch, apply_calibration(rgb[ch], ch, led));
            }
        }

        // Encodes frame to different bit layouts for each wing for shifting, one per BCM bit-plane
        for (int plane = 0; plane < BCM_BITS; plane++) {
            frame_buffer_to_words(frame, frame, plane, image_lhs_words[col][plane], image_rhs_words[col][plane]);
        }
    }
}

// Sets one column from raw 0-255 RGB, e.g. pushed live over SPI
void image_source_set_column(int col, const uint8_t rgb[LEDS_PER_WING][CHANNELS]) {
    // Remember the raw (uncalibrated) values so a later calibration change applies to raw frame
    memmove(last_frame[col], rgb, sizeof(last_frame[col])); // Not memcpy as source and destination may be same if called from set_calibration
    has_uploaded_frame = true;

    uint8_t frame[LEDS_PER_WING][CHANNELS];
    frame_buffer_clear(frame);

    for (int led = 0; led < LEDS_PER_WING; led++) {
        for (int ch = 0; ch < CHANNELS; ch++) {
            frame_buffer_set(frame, led, ch, apply_calibration(rgb[led][ch], ch, led));
        }
    }

    // Encodes frame to different bit layouts for each wing for shifting, one per BCM bit-plane
    for (int plane = 0; plane < BCM_BITS; plane++) {
        frame_buffer_to_words(frame, frame, plane, image_lhs_words[col][plane], image_rhs_words[col][plane]);
    }
}
