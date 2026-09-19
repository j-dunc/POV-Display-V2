// spi_link: RP2350 side of the RP2350<->ESP32 SPI link. Runs the SPI slave
// command loop (ready-line handshake, command dispatch) on core 1

#include "spi_link.h"
#include "pins.h"
#include "hall_sensor.h"
#include "image_source.h"
#include "animation.h"
#include "animation_store.h"
#include "render.h"

#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "pico/time.h"

#include "spi_protocol.h"

#define SPI_TRANSFER_TIMEOUT_US 50000


static int rx_dma_chan = -1;
static int tx_dma_chan = -1;

// Returns false if timeout occured. Also controls ready line handshake
static bool spi_read_with_timeout(uint8_t *dst, size_t len) {
    gpio_put(PIN_RP_ESP32_GPIO, 1); // ready to receive

    // Receive rx bytes with DMA
    dma_channel_config c = dma_channel_get_default_config(rx_dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, false); // always the same SPI FIFO register
    channel_config_set_write_increment(&c, true); // walk forward through dst
    channel_config_set_dreq(&c, spi_get_dreq(spi1, false)); // paced by bytes actually arriving
    dma_channel_configure(rx_dma_chan, &c, dst, &spi_get_hw(spi1)->dr, len, true); // true: start now

    absolute_time_t deadline = make_timeout_time_us(SPI_TRANSFER_TIMEOUT_US);
    while (dma_channel_is_busy(rx_dma_chan)) {
        if (time_reached(deadline)) {
            dma_channel_abort(rx_dma_chan);
            spi_link_init(); // reset rather than leave core 1 hanging (also lowers ready)
            return false;
        }
    }

    gpio_put(PIN_RP_ESP32_GPIO, 0); // busy
    return true;
}

// Returns false if timeout occured. Also controls ready line handshake
static bool spi_write_with_timeout(const uint8_t *src, size_t len) {
    // Send Tx bytes with DMA
    dma_channel_config c = dma_channel_get_default_config(tx_dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, true); // walk forward through src
    channel_config_set_write_increment(&c, false); // always the same SPI FIFO register
    channel_config_set_dreq(&c, spi_get_dreq(spi1, true)); // paced by FIFO space freeing up
    dma_channel_configure(tx_dma_chan, &c, &spi_get_hw(spi1)->dr, src, len, true); // true: start now

    gpio_put(PIN_RP_ESP32_GPIO, 1); // ready with the reply

    absolute_time_t deadline = make_timeout_time_us(SPI_TRANSFER_TIMEOUT_US);
    while (dma_channel_is_busy(tx_dma_chan)) {
        if (time_reached(deadline)) {
            dma_channel_abort(tx_dma_chan);
            spi_link_init(); // reset rather than leave core 1 hanging (also lowers ready)
            return false;
        }
    }

    while (spi_get_hw(spi1)->sr & SPI_SSPSR_BSY_BITS) tight_loop_contents(); // wait until spi busy flag is cleared
    while (spi_is_readable(spi1)) (void)spi_get_hw(spi1)->dr; // drain leftover rx bytes (SPI is full duplex despite not needed)
    spi_get_hw(spi1)->icr = SPI_SSPICR_RORIC_BITS; // clear overrun flag (rx overrun fine as not used)

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

    // Only claim once (this function runs on spi-link resets also)
    if (rx_dma_chan < 0) rx_dma_chan = dma_claim_unused_channel(true);
    if (tx_dma_chan < 0) tx_dma_chan = dma_claim_unused_channel(true);
}

// True between ANIM_BEGIN and ANIM_END
static bool uploading = false;

// When the last upload command landed
static absolute_time_t last_upload_activity;
#define UPLOAD_IDLE_TIMEOUT_US (5 * 1000 * 1000)

// Called if an upload breaks off part way - so display doesn't stay blanked and uploading isnt blocked
static void abort_upload(void) {
    uploading = false;
    render_set_blanked(false);
}

void spi_link_command_loop(void) {
    spi_link_init();

    while (true) {

        // Read CMD from ESP32
        uint8_t cmd;
        if (!spi_read_with_timeout(&cmd, 1)) {
            if (uploading) {
                // Don't build frames mid upload - but don't stay parked if the upload died
                if (absolute_time_diff_us(last_upload_activity, get_absolute_time()) > UPLOAD_IDLE_TIMEOUT_US) abort_upload();
            } else {
                animation_build_next_frame(); // No CMD from ESP32 so spend idle time building next frame
            }
            continue;
        }

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
                    animation_store_clear(); // a pushed frame takes the display over from any animation
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

                image_source_set_calibration(payload.r_scale, payload.g_scale, payload.b_scale, payload.gamma, payload.radial_gamma, payload.max_duty_fraction, payload.dither_enabled != 0.0f, (int)bcm_bits_f);
                render_set_revs_per_frame((int)payload.revs_per_frame);
                break;
            }

            case CMD_GET_CALIBRATION: {
                calibration_payload_t payload;
                image_source_get_calibration(&payload);
                payload.revs_per_frame = (float)render_get_revs_per_frame(); // lives in render, not image_source
                if (!spi_write_with_timeout((uint8_t *)&payload, sizeof(payload))) continue;
                break;
            }

            case CMD_ANIM_BEGIN: {
                uint8_t payload[ANIM_BEGIN_PAYLOAD_SIZE];
                if (!spi_read_with_timeout(payload, sizeof(payload))) continue;

                uint16_t frame_count = payload[0] | (payload[1] << 8); // reassemble bytes (little-endian)

                render_set_blanked(true); // blank for the whole upload
                if (animation_store_begin_write(frame_count)) {
                    uploading = true;
                    last_upload_activity = get_absolute_time();
                } else {
                    render_set_blanked(false); // nothing to upload into - don't stay blanked
                }
                break;
            }

            case CMD_ANIM_COLUMN: {
                uint8_t payload[FRAME_COLUMN_PAYLOAD_SIZE];
                if (!spi_read_with_timeout(payload, sizeof(payload))) {
                    abort_upload(); // upload failed - abort
                    continue;
                }
                if (!uploading) break; // column outside an upload - ignore

                last_upload_activity = get_absolute_time();
                int col = payload[0] | (payload[1] << 8);
                animation_store_write_column(col, (const uint8_t (*)[CHANNELS])&payload[2]);
                break;
            }

            case CMD_ANIM_END: {
                if (uploading) {
                    animation_store_end_write();
                    uploading = false;
                    render_set_blanked(false);
                }
                break;
            }

            case CMD_SHOW_TEST_PATTERN: {
                uint8_t pattern_id;
                if (!spi_read_with_timeout(&pattern_id, TEST_PATTERN_PAYLOAD_SIZE)) continue;

                animation_store_clear(); // a test pattern takes the display over from any animation
                image_source_show_test_pattern(pattern_id);
                break;
            }

            default:
                break; // unknown command, ignore
        }
    }
}
