// animation: writes the built-in test animation to flash on boot, then plays it -
// reads each frame's raw RGB straight out of flash, calibrates it and hands the
// columns to frame_buffer

#include "animation.h"
#include "animation_store.h"
#include "image_source.h"
#include "frame_buffer.h"
#include "render.h"

#include "pico/stdlib.h"

// Built-in test animation: a rainbow that turns exactly one full revolution over the whole loop
#define TEST_FRAME_COUNT 30
#define TEST_REVS_PER_FRAME 2

static void hue_to_rgb(float hue_deg, uint8_t rgb[CHANNELS]) {
    int sector = (int)(hue_deg / 60.0f);
    float sector_frac = (hue_deg / 60.0f) - sector;
    float ramp_frac = (sector % 2 == 0) ? sector_frac : (1.0f - sector_frac);
    uint8_t ramp = (uint8_t)(255 * ramp_frac + 0.5f);

    switch (sector) {
        case 0: rgb[RED] = 255; rgb[GREEN] = ramp; rgb[BLUE] = 0; break;
        case 1: rgb[RED] = ramp; rgb[GREEN] = 255; rgb[BLUE] = 0; break;
        case 2: rgb[RED] = 0; rgb[GREEN] = 255; rgb[BLUE] = ramp; break;
        case 3: rgb[RED] = 0; rgb[GREEN] = ramp; rgb[BLUE] = 255; break;
        case 4: rgb[RED] = ramp; rgb[GREEN] = 0; rgb[BLUE] = 255; break;
        default: rgb[RED] = 255; rgb[GREEN] = 0; rgb[BLUE] = ramp; break;
    }
}

static void fill_test_column(uint32_t frame_index, int col, uint8_t dest[LEDS_PER_WING][CHANNELS]) {
    int shift = (int)((frame_index * (uint32_t)NUM_COLUMNS) / TEST_FRAME_COUNT);
    int pattern_col = (col + shift) % NUM_COLUMNS;

    uint8_t rgb[CHANNELS];
    hue_to_rgb(360.0f * pattern_col / NUM_COLUMNS, rgb);

    for (int led = 0; led < LEDS_PER_WING; led++) {
        for (int ch = 0; ch < CHANNELS; ch++) {
            dest[led][ch] = rgb[ch];
        }
    }
}

void animation_init(void) {
    if (!animation_store_begin_write(TEST_FRAME_COUNT)) return;

    uint8_t rgb[LEDS_PER_WING][CHANNELS];
    for (uint32_t frame = 0; frame < TEST_FRAME_COUNT; frame++) {
        for (int col = 0; col < NUM_COLUMNS; col++) {
            fill_test_column(frame, col, rgb);
            animation_store_write_column(col, rgb);
        }
    }
    animation_store_end_write();

    render_set_revs_per_frame(TEST_REVS_PER_FRAME);
}

// Reads one stored frame out of flash, calibrates every column and stages it in
// the frame buffer's back buffer
static void build_frame(uint32_t index) {
    int bcm_bits = image_source_get_bcm_bits();

    uint8_t raw_column[LEDS_PER_WING][CHANNELS];
    uint8_t column_levels[LEDS_PER_WING][CHANNELS];
    for (int col = 0; col < NUM_COLUMNS; col++) {
        animation_store_read_column(index, col, raw_column);

        for (int led = 0; led < LEDS_PER_WING; led++) {
            for (int ch = 0; ch < CHANNELS; ch++) {
                column_levels[led][ch] = image_source_apply_calibration(raw_column[led][ch], ch, led, col);
            }
        }

        frame_buffer_publish_column(col, column_levels, bcm_bits);
    }
}

void animation_task(void) {
    uint32_t frame_count = animation_store_frame_count();

    uint32_t index = 0;

    while (true) {
        if (frame_count == 0) { // nothing stored - leave the display blank rather than exiting core 1
            tight_loop_contents();
            continue;
        }

        // Wait for current built frame to be swapped in before filling back buffer again
        while (!frame_buffer_back_buffer_free()) {
            tight_loop_contents();
        }

        build_frame(index);
        index = (index + 1) % frame_count; // loop animation
    }
}
