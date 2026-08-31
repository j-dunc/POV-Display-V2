#include <stdio.h>
#include "pico/stdlib.h"
#include "board_init.h"
#include "led_shift.h"
#include "pins.h"


// All-on/all-off test for the PIO shift driver

#define PWM_PERIOD_US    10
#define PWM_DUTY_PERCENT 5 // Max safe level is 20% from testing to avoid brownout
#define PWM_ON_US        (PWM_PERIOD_US * PWM_DUTY_PERCENT / 100)
#define PWM_OFF_US       (PWM_PERIOD_US - PWM_ON_US)
#define ON_DURATION_MS   1000
#define OFF_DURATION_MS  1000

int main(void)
{
    stdio_init_all();
    board_init();
    led_shift_init();

    /*
    Only the low 11 (LHS) / 9 (RHS) bits of each word are ever read by
    the PIO programs so all-1s lights every real data pin. The rest, including
    the two LHS gap bits (5=HE_1, 9=OE), are don't-cares.
    */
    uint32_t lhs_words_on[16], rhs_words_on[16];
    uint32_t lhs_words_off[16] = {0}, rhs_words_off[16] = {0};
    for (int i = 0; i < 16; i++) {
        lhs_words_on[i] = 0xFFFFFFFFu;
        rhs_words_on[i] = 0xFFFFFFFFu;
    }

    while (true) {
        led_shift_send(lhs_words_on, rhs_words_on);
        for (int i = 0; i < (ON_DURATION_MS * 1000) / PWM_PERIOD_US; i++) {
            gpio_put(PIN_OE, 0);
            sleep_us(PWM_ON_US);
            gpio_put(PIN_OE, 1);
            sleep_us(PWM_OFF_US);
        }

        led_shift_send(lhs_words_off, rhs_words_off);
        sleep_ms(OFF_DURATION_MS);
    }
}

