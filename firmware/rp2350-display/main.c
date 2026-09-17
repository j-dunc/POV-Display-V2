// main: entry point. Boots the SPI link on core 1, initialises drivers and
// render state, then runs the render loop on core 0

#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "led_shift.h"
#include "hall_sensor.h"
#include "led_map.h"
#include "shift_encoder.h"
#include "image_source.h"
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
    shift_encoder_init();
    rebuild_calibration();
    render_init();

    while (true) {
        render_update();
    }
}
