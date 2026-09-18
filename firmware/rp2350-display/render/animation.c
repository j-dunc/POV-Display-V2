// animation: plays the animation stored in flash - reads each frame's raw RGB
// straight out of flash, calibrates it and hands the columns to frame_buffer

#include "animation.h"
#include "animation_store.h"
#include "image_source.h"
#include "frame_buffer.h"

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

void animation_build_next_frame(void) {
    uint32_t frame_count = animation_store_frame_count();
    if (frame_count == 0) return; // nothing stored - leave the display blank

    // Wait for current built frame to be swapped in before filling back buffer again
    if (!frame_buffer_back_buffer_free()) return;

    static uint32_t index = 0; // note as static this only resets on first run
    if (index >= frame_count) index = 0; // an upload may have shortened the animation

    build_frame(index);
    index = (index + 1) % frame_count; // loop animation
}
