#include <stdio.h>

#include "pico/stdlib.h"
#include "board_init.h"
#include "led_shift.h"
#include "hall_sensor.h"
#include "led_map.h"
#include "render.h"

#define TEST 1 // 1 for true

int main(void)
{
    stdio_init_all();
    board_init();
    led_shift_init();
    hall_sensor_init();
    build_led_map();
    render_init();

#if TEST
    float max_rpm = 0.0f;
    uint64_t last_print_us = 0;
#endif

    while (true) {
        render_update();

#if TEST
        // Print max RPM for bench testing
        float rpm = hall_sensor_get_rpm();
        if (rpm > max_rpm) max_rpm = rpm;

        uint64_t now = time_us_64();
        if (now - last_print_us >= 1000000) {
            printf("Max RPM: %.1f\n", max_rpm);
            last_print_us = now;
        }
#endif
    }
}
