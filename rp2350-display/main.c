#include "pico/stdlib.h"
#include "board_init.h"
#include "hall_sensor.h"

#define DUMP_INTERVAL_MS 3000

int main(void)
{
    stdio_init_all();
    board_init();
    hall_sensor_init();

    while (true) {
        hall_sensor_dump_log();
        sleep_ms(DUMP_INTERVAL_MS);
    }
}
