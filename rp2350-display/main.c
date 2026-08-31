#include <stdio.h>
#include "pico/stdlib.h"
#include "board_init.h"


int main()
{
    stdio_init_all();
    board_init();

    while (true) {
        printf("Hello, world!\n");
        sleep_ms(1000);
    }
}
