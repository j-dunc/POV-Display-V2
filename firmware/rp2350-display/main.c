#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "led_shift.h"
#include "hall_sensor.h"
#include "led_map.h"
#include "render.h"
#include "spi_link.h"

int main(void)
{
    stdio_init_all();

    // Blocking loop on core 1, handling SPI communication with ESP32
    multicore_launch_core1(spi_link_command_loop);

    led_shift_init();
    hall_sensor_init();
    build_led_map();
    render_init();

    while (true) {
        render_update();
    }
}
