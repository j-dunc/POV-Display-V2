#include "image_source.h"
#include "led_map.h"
#include "frame_buffer.h"

#include <stdbool.h>

// Stores the entire image, word array per column
uint16_t image_lhs_words[NUM_COLUMNS][16];
uint16_t image_rhs_words[NUM_COLUMNS][16];

void image_source_build(void) {

    // Column content is a function of angle only, not which wing so frame same for each wing
    bool frame[LEDS_PER_WING][CHANNELS];

    for (int col = 0; col < NUM_COLUMNS; col++) {
        frame_buffer_clear(frame);

        // First half of the columns red, second half green 
        // Every 5 rows to avoid brownout from overcurrent
        int channel = (col < NUM_COLUMNS / 2) ? RED : GREEN;
        for (int led = 0; led < LEDS_PER_WING; led++) {
            if (led % 5 == 0) {
                frame_buffer_set(frame, led, channel, true);
            }
        }

        // Encodes frame to different bit layouts for each wing for shifting
        frame_buffer_to_words(frame, frame, image_lhs_words[col], image_rhs_words[col]);
    }
}
