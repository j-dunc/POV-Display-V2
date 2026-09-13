#include "spi_link.h"
#include "pins.h"
#include "hall_sensor.h"
#include "render.h"

#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/time.h"

#include "spi_protocol.h"

void spi_link_init(void) {
    // Setup SPI1 peripheral as slave
    spi_init(spi1, 1000000); // Note baudrate is irrelevant as set by ESP32
    spi_set_slave(spi1, true);

    // 8 bits per word, MSB first, CPOL 0, CPHA 1 (ESP mode 1 - 0 doesn't work with multibyte transfers)
    // These must match ESP32
    spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_1, SPI_MSB_FIRST);

    // Init SPI GPIO pins
    gpio_set_function(PIN_RP_SPI_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_RP_SPI_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_RP_SPI_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_RP_SPI_CSN, GPIO_FUNC_SPI);

    // Ready line to ESP32
    gpio_init(PIN_RP_ESP32_GPIO);
    gpio_set_dir(PIN_RP_ESP32_GPIO, GPIO_OUT);
    gpio_put(PIN_RP_ESP32_GPIO, 0);
}

void spi_link_command_loop(void) {
    spi_link_init();

    while (true) {

        // Read CMD from ESP32
        uint8_t cmd;
        gpio_put(PIN_RP_ESP32_GPIO, 1); // ready for a command
        spi_read_blocking(spi1, 0, &cmd, 1);
        gpio_put(PIN_RP_ESP32_GPIO, 0); // busy - handling it

        // Carry out CMD
        switch (cmd) {
            case CMD_PING: {
                // Echo the command byte back
                uint8_t resp = CMD_PING;
                gpio_put(PIN_RP_ESP32_GPIO, 1); // ready with the reply
                spi_write_blocking(spi1, &resp, 1);
                gpio_put(PIN_RP_ESP32_GPIO, 0);
                break;
            }

            case CMD_GET_STATUS: {
                float rpm = hall_sensor_get_rpm();
                spi_status_t status = {
                    .rpm = rpm,
                    .spinning = rpm >= MIN_READABLE_RPM,
                    .uptime_ms = to_ms_since_boot(get_absolute_time()),
                };
                gpio_put(PIN_RP_ESP32_GPIO, 1); // ready with the reply
                spi_write_blocking(spi1, (uint8_t *)&status, sizeof(status)); // Note pointer to spi_status_t type is cast to pointer to raw bytes
                gpio_put(PIN_RP_ESP32_GPIO, 0);
                break;
            }

            default:
                break; // unknown command, ignore
        }
    }
}
