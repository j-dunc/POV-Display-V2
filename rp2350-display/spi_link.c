#include "spi_link.h"
#include "pins.h"

#include "hardware/spi.h"
#include "hardware/gpio.h"


void spi_link_init(void) {
    // Setup SPI1 peripheral as slave
    spi_init(spi1, 1000000);
    spi_set_slave(spi1, true);
    spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST); // 8 bits per word, mode 0, MSB shifted first - must match ESP32 config

    // Init SPI GPIO pins
    gpio_set_function(PIN_RP_SPI_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_RP_SPI_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_RP_SPI_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_RP_SPI_CSN, GPIO_FUNC_SPI);
}

void spi_link_echo_test(void) {
    uint8_t tx = 0xA5; // Test echo byte
    uint8_t rx;
    while (true) {
        spi_write_read_blocking(spi1, &tx, &rx, 1);
    }
}
