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
#include "animation.h"

int main(void)
{
    stdio_init_all();

    led_shift_init();
    hall_sensor_init();
    build_led_map();
    shift_encoder_init();
    rebuild_calibration();
    render_init();
    animation_init();

    // TEMPORARY: animation playback in place of the SPI link - no ESP32 involved
    multicore_launch_core1(animation_task);

    while (true) {
        render_update();
    }
}
