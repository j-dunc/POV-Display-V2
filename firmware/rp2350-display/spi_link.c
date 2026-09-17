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
// Also controls ready line handshake
static bool spi_read_with_timeout(uint8_t *dst, size_t len) {
    absolute_time_t deadline = make_timeout_time_us(SPI_TRANSFER_TIMEOUT_US);
    gpio_put(PIN_RP_ESP32_GPIO, 1); // ready to receive

    for (size_t i = 0; i < len; i++) {
        spi_get_hw(spi1)->dr = 0; // Write dummy tx byte to data register
        while (!spi_is_readable(spi1)) { // Check if transfer complete
            if (time_reached(deadline)) {
                spi_link_init(); // reset rather than leave core 1 hanging (also lowers ready)
                return false;
            }
        }
        dst[i] = (uint8_t)spi_get_hw(spi1)->dr; // Read byte
    }

    gpio_put(PIN_RP_ESP32_GPIO, 0); // busy
    return true;
}

// Fills FIFO before raising ready to ensure no garbage is clocked out
static bool spi_write_with_timeout(const uint8_t *src, size_t len) {
    absolute_time_t deadline = make_timeout_time_us(SPI_TRANSFER_TIMEOUT_US);

    size_t queued = 0;
    // Loop until all sent or FIFO full
    while (queued < len && spi_is_writable(spi1)) {
        spi_get_hw(spi1)->dr = src[queued++];
    }
    gpio_put(PIN_RP_ESP32_GPIO, 1); // ready with the reply

    for (size_t i = 0; i < len; i++) {
        if (i >= queued) { // anything beyond the initial burst still needs pacing
            while (!spi_is_writable(spi1)) {
                if (time_reached(deadline)) {
                    spi_link_init(); // also lowers ready
                    return false;
                }
            }
            spi_get_hw(spi1)->dr = src[i]; // Write byte
        }

        // Discard unwanted rx bytes
        while (!spi_is_readable(spi1)) {
            if (time_reached(deadline)) {
                spi_link_init(); // also lowers ready
                return false;
            }
        }
        (void)spi_get_hw(spi1)->dr;
    }

    gpio_put(PIN_RP_ESP32_GPIO, 0); // busy
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
        if (!spi_read_with_timeout(&cmd, 1)) continue;

        // Carry out CMD
        switch (cmd) {
            case CMD_PING: {
                // Echo the command byte back
                uint8_t resp = CMD_PING;
                if (!spi_write_with_timeout(&resp, 1)) continue;
                break;
            }

            case CMD_GET_STATUS: {
                spi_status_t status = {
                    .rpm = hall_sensor_get_rpm(),
                    .uptime_ms = to_ms_since_boot(get_absolute_time()),
                };
                if (!spi_write_with_timeout((uint8_t *)&status, sizeof(status))) continue; // Note pointer to spi_status_t type is cast to pointer to raw bytes
                break;
            }

            case CMD_PUSH_COLUMN: {
                uint8_t payload[FRAME_COLUMN_PAYLOAD_SIZE];
                if (!spi_read_with_timeout(payload, sizeof(payload))) continue;

                int col = payload[0] | (payload[1] << 8); // reassemble bytes (little-endian)
                if (col >= 0 && col < NUM_COLUMNS) {
                    image_source_set_column(col, (const uint8_t (*)[CHANNELS])&payload[2]); // Note we send a pointer to the 3rd element (skipping column index), and cast this to a pointer to arrays of 3 (arrays of 3 channel LEDs)
                }
                break;
            }

            case CMD_SET_CALIBRATION: {
                calibration_payload_t payload;
                if (!spi_read_with_timeout((uint8_t *)&payload, sizeof(payload))) continue;

                // Clamp values before casting
                float bcm_bits_f = payload.bcm_bits;
                if (!(bcm_bits_f >= 1.0f)) bcm_bits_f = 1.0f; // minimum floor is 1, also catches NaN
                if (bcm_bits_f > (float)BCM_BITS_MAX) bcm_bits_f = (float)BCM_BITS_MAX;

                image_source_set_calibration(payload.r_scale, payload.g_scale, payload.b_scale, payload.gamma, payload.radial_gamma, payload.dither_enabled != 0.0f, (int)bcm_bits_f);
                render_set_max_duty_fraction(payload.max_duty_fraction);
                break;
            }

            case CMD_GET_CALIBRATION: {
                calibration_payload_t payload;
                image_source_get_calibration(&payload);
                payload.max_duty_fraction = render_get_max_duty_fraction();
                if (!spi_write_with_timeout((uint8_t *)&payload, sizeof(payload))) continue;
                break;
            }

            case CMD_SHOW_TEST_PATTERN: {
                uint8_t pattern_id;
                if (!spi_read_with_timeout(&pattern_id, TEST_PATTERN_PAYLOAD_SIZE)) continue;

                image_source_show_test_pattern(pattern_id);
                break;
            }

            default:
                break; // unknown command, ignore
        }
    }
}
