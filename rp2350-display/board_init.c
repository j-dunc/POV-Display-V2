#include "board_init.h"
#include "pins.h"
#include "pico/stdlib.h"

static void init_output_low(uint pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, 0);
}

static void init_input(uint pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
}

void board_init(void) {
    // Set all active high pins to low
    for (int i = 0; i < 9; i++) {
        init_output_low(LHS_DATA_PINS[i]);
        init_output_low(RHS_DATA_PINS[i]);
    }

    init_output_low(PIN_LHS_CLK);
    init_output_low(PIN_RHS_CLK);
    init_output_low(PIN_LE);

    // OE is active low
    gpio_init(PIN_OE);
    gpio_set_dir(PIN_OE, GPIO_OUT);
    gpio_put(PIN_OE, 1);

    // HE open-collector so pull up
    init_input(PIN_HE_0);
    gpio_pull_up(PIN_HE_0);
    init_input(PIN_HE_1);
    gpio_pull_up(PIN_HE_1);

    // Leave SPI1 peripheral as inputs until used
    init_input(PIN_RP_SPI_MOSI);
    init_input(PIN_RP_SPI_CSN);
    init_input(PIN_RP_SPI_SCK);
    init_input(PIN_RP_SPI_MISO);

    init_input(PIN_RP_ESP32_GPIO);
}