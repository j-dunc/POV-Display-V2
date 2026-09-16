#include "spi_link.h"
#include "pins.h"
#include "hall_sensor.h"
#include "render.h"
#include "image_source.h"

#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/time.h"

#include "spi_protocol.h"

#define SPI_TRANSFER_TIMEOUT_US 50000

// SPI functions with timeout so failed transfers do not hang core
// Returns false if timeout occured
static bool spi_read_with_timeout(uint8_t *dst, size_t len) {
    absolute_time_t deadline = make_timeout_time_us(SPI_TRANSFER_TIMEOUT_US);

    for (size_t i = 0; i < len; i++) {
        spi_get_hw(spi1)->dr = 0; // Write dummy tx byte to data register
        while (!spi_is_readable(spi1)) { // Check if transfer complete
            if (time_reached(deadline)) {
                spi_link_init(); // reset rather than leave core 1 hanging
                return false;
            }
        }
        dst[i] = (uint8_t)spi_get_hw(spi1)->dr; // Read byte
    }
    return true;
}

static bool spi_write_with_timeout(const uint8_t *src, size_t len) {
    absolute_time_t deadline = make_timeout_time_us(SPI_TRANSFER_TIMEOUT_US);

    for (size_t i = 0; i < len; i++) {
        while (!spi_is_writable(spi1)) {
            if (time_reached(deadline)) {
                spi_link_init();
                return false;
            }
        }
        spi_get_hw(spi1)->dr = src[i]; // Write byte

        // Discard unwanted rx bytes
        while (!spi_is_readable(spi1)) {
            if (time_reached(deadline)) {
                spi_link_init();
                return false;
            }
        }
        (void)spi_get_hw(spi1)->dr;
    }
    return true;
}

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
        if (!spi_read_with_timeout(&cmd, 1)) continue;
        gpio_put(PIN_RP_ESP32_GPIO, 0); // busy - handling it

        // Carry out CMD
        switch (cmd) {
            case CMD_PING: {
                // Echo the command byte back
                uint8_t resp = CMD_PING;
                gpio_put(PIN_RP_ESP32_GPIO, 1); // ready with the reply
                if (!spi_write_with_timeout(&resp, 1)) continue;
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
                if (!spi_write_with_timeout((uint8_t *)&status, sizeof(status))) continue; // Note pointer to spi_status_t type is cast to pointer to raw bytes
                gpio_put(PIN_RP_ESP32_GPIO, 0);
                break;
            }

            case CMD_PUSH_COLUMN: {
                uint8_t payload[FRAME_COLUMN_PAYLOAD_SIZE];
                gpio_put(PIN_RP_ESP32_GPIO, 1); // ready for the column payload
                if (!spi_read_with_timeout(payload, sizeof(payload))) continue;
                gpio_put(PIN_RP_ESP32_GPIO, 0);

                int col = payload[0] | (payload[1] << 8); // reassemble bytes (little-endian)
                if (col >= 0 && col < NUM_COLUMNS) {
                    image_source_set_column(col, (const uint8_t (*)[CHANNELS])&payload[2]); // Note we send a pointer to the 3rd element (skipping column index), and cast this to a pointer to arrays of 3 (arrays of 3 channel LEDs)
                }
                break;
            }

            case CMD_SET_CALIBRATION: {
                calibration_payload_t payload;
                gpio_put(PIN_RP_ESP32_GPIO, 1); // ready for the calibration payload
                if (!spi_read_with_timeout((uint8_t *)&payload, sizeof(payload))) continue;
                gpio_put(PIN_RP_ESP32_GPIO, 0);

                image_source_set_calibration(payload.r_scale, payload.g_scale, payload.b_scale, payload.gamma, payload.radial_gamma);
                break;
            }

            default:
                break; // unknown command, ignore
        }
    }
}
