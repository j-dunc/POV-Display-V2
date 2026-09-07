#include <stdio.h>
#include "pico/stdlib.h"
#include "board_init.h"
#include "hall_sensor.h"

#define PRINT_INTERVAL_MS 500

int main(void)
{
    stdio_init_all();
    board_init();
    hall_sensor_init();

    while (true) {
        printf("angle=%.1f deg period=%lu us rpm=%.1f\n",
               hall_sensor_get_angle_deg(time_us_64()),
               (unsigned long)hall_sensor_get_period_us(),
               hall_sensor_get_rpm());
        sleep_ms(PRINT_INTERVAL_MS);
    }
}
