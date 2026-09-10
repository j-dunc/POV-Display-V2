#include "pico/stdlib.h"
#include "board_init.h"
#include "led_shift.h"
#include "hall_sensor.h"
#include "led_map.h"
#include "render.h"

int main(void)
{
    stdio_init_all();
    board_init();
    led_shift_init();
    hall_sensor_init();
    build_led_map();
    render_init();

    while (true) {
        render_update();
    }
}
