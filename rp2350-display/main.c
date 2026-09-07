#include <stdio.h>
#include "pico/stdlib.h"
#include "board_init.h"
#include "led_shift.h"
#include "frame_buffer.h"
#include "pins.h"

#define HOLD_MS 250

int main(void)
{
    stdio_init_all();
    board_init();
    led_shift_init();
    build_led_map();
    gpio_put(PIN_OE, 0);

    bool lhs_frame[LEDS_PER_WING][CHANNELS];
    bool rhs_frame[LEDS_PER_WING][CHANNELS];
    uint32_t lhs_words[16], rhs_words[16];

    while (true) {
        for (int led = 0; led < LEDS_PER_WING; led++) {
            for (int ch = 0; ch < CHANNELS; ch++) {
                frame_buffer_clear(lhs_frame);
                frame_buffer_clear(rhs_frame);
                frame_buffer_set(lhs_frame, led, ch, true);
                frame_buffer_set(rhs_frame, led, ch, true);

                frame_buffer_to_words(lhs_frame, rhs_frame, lhs_words, rhs_words);
                led_shift_send(lhs_words, rhs_words);

                printf("led %d channel %d\n", led, ch);
                sleep_ms(HOLD_MS);
            }
        }
    }
}
