// spi_link: ESP32 side of the RP2350<->ESP32 SPI link. ESP32 is the SPI
// master; sends command/response transactions to push frames and calibration

#include "spi_link.h"
#include "pins.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <string.h>

static const char *TAG = "spi_link";
static spi_device_handle_t spi_dev;
static SemaphoreHandle_t ready_sem;

#define SPI_LINK_HOST SPI2_HOST
// sspclk = tied to system clock, sspclkin = spi clk from esp32
// sspclk >= 12 x sspclkin for slave mode for PL022
// so max SPI clk is 150/12 = 12.5 MHz
#define SPI_LINK_CLOCK_HZ (10 * 1000 * 1000)

// How long to wait for the RP2350's ready line before giving up on a command
#define READY_TIMEOUT_MS 500

// ANIM_BEGIN erases flash before raising ready flag - so much longer timeout needed
#define ANIM_ERASE_TIMEOUT_MS 180000


// Interrupt fires on the rising edge of the RP2350's ready line
static void IRAM_ATTR ready_isr_handler(void *arg) {
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(ready_sem, &woken); // Send semaphore token
    if (woken) {
        portYIELD_FROM_ISR(); // Switch to wait_ready task instantly
    }
}

void spi_link_init(void) {
    // SPI bus config and init
    spi_bus_config_t bus_cfg = {
        .miso_io_num = PIN_SPI_MISO,
        .mosi_io_num = PIN_SPI_MOSI,
        .sclk_io_num = PIN_SPI_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI_LINK_HOST, &bus_cfg, SPI_DMA_CH_AUTO)); // Must use DMA as columns are larger than FIFO

    // SPI device config and init
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = SPI_LINK_CLOCK_HZ,
        .mode = 1, // CPOL=0, CPHA=1 - must match RP2350, note mode 0 didn't work with multi-byte transfers
        .spics_io_num = PIN_SPI_CS,
        .queue_size = 1,
        .cs_ena_posttrans = 3, // hold CS briefly after each transfer so the slave doesn't miss the last bit
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI_LINK_HOST, &dev_cfg, &spi_dev));

    // Setup ready line GPIO and ISR
    gpio_config_t ready_cfg = {
        .pin_bit_mask = 1ULL << PIN_RP_ESP_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE, // Rising edge triggered
    };
    ready_sem = xSemaphoreCreateBinary();
    ESP_ERROR_CHECK(gpio_config(&ready_cfg));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_RP_ESP_GPIO, ready_isr_handler, NULL));

    // Assume RP2350 is already ready - in case ready rising edge was missed
    xSemaphoreGive(ready_sem);
}

    // Blocks until semaphore token received or timeout reached
    // Return true if token received, false if timeout
    // After the token is received, we check ready line is still high in case of a stale token
static bool wait_ready_for(uint32_t timeout_ms) {
    for (int attempt = 0; attempt < 3; attempt++) {
        if (gpio_get_level(PIN_RP_ESP_GPIO)) return true;
        if (xSemaphoreTake(ready_sem, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) return false;
    }
    return false;
}

// For commands that are two SPI transactions:
// send command byte, wait for RP2350 ready line, then clock reply back out
static bool spi_link_transact(uint8_t cmd, uint8_t *resp, size_t resp_len) {
    // Wait for RP2350 ready
    if (!wait_ready_for(READY_TIMEOUT_MS)) {
        ESP_LOGW(TAG, "RP2350 not ready for command 0x%02x", cmd);
        return false;
    }

    // Send CMD transaction (no rx) and handle errors
    spi_transaction_t req_t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    esp_err_t err = spi_device_transmit(spi_dev, &req_t);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "request transaction failed: %s", esp_err_to_name(err));
        return false;
    }

    // Wait for RP2350 ready
    if (!wait_ready_for(READY_TIMEOUT_MS)) {
        ESP_LOGW(TAG, "no reply staged for command 0x%02x", cmd);
        return false;
    }

    // Send transaction to receive response (no tx) and handle errors
    spi_transaction_t resp_t = {
        .length = resp_len * 8,
        .rx_buffer = resp,
    };
    err = spi_device_transmit(spi_dev, &resp_t);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "response transaction failed: %s", esp_err_to_name(err));
        return false;
    }

    return true;
}

// Sends a commmand byte and payload (if there is one)
// Both wait for RP2350 ready line
// No reply read back
static bool send_command(uint8_t cmd, const void *payload, size_t len) {
    // Send command when ready and check for errors
    if (!wait_ready_for(READY_TIMEOUT_MS)) {
        ESP_LOGW(TAG, "RP2350 not ready for command 0x%02x", cmd);
        return false;
    }
    WORD_ALIGNED_ATTR uint8_t cmd_byte = cmd; // Must be word aligned for DMA
    spi_transaction_t cmd_t = {
        .length = 8,
        .tx_buffer = &cmd_byte,
    };
    esp_err_t err = spi_device_transmit(spi_dev, &cmd_t);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "command 0x%02x failed: %s", cmd, esp_err_to_name(err));
        return false;
    }

    // If there is a payload, send it when ready and check for errors
    if (len == 0) return true;

    if (!wait_ready_for(READY_TIMEOUT_MS)) {
        ESP_LOGW(TAG, "RP2350 not ready for command 0x%02x payload", cmd);
        return false;
    }
    spi_transaction_t payload_t = {
        .length = len * 8,
        .tx_buffer = payload,
    };
    err = spi_device_transmit(spi_dev, &payload_t);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "command 0x%02x payload failed: %s", cmd, esp_err_to_name(err));
        return false;
    }
    return true;
}

bool spi_link_animation_begin(uint16_t frame_count) {
    WORD_ALIGNED_ATTR uint8_t payload[ANIM_BEGIN_PAYLOAD_SIZE]; // Must be word aligned for DMA
    payload[0] = frame_count & 0xFF; // Lower byte
    payload[1] = (frame_count >> 8) & 0xFF; // Upper byte

    ESP_LOGI(TAG, "animation upload starting: %u frames", frame_count);
    if (!send_command(CMD_ANIM_BEGIN, payload, sizeof(payload))) return false;

    // RP2350 is erasing flash now and won't raise ready again until it's done
    if (!wait_ready_for(ANIM_ERASE_TIMEOUT_MS)) {
        ESP_LOGW(TAG, "RP2350 did not finish erasing in time");
        return false;
    }
    return true;
}

bool spi_link_animation_column(uint16_t col, const uint8_t rgb[LEDS_PER_WING][CHANNELS]) {
    WORD_ALIGNED_ATTR uint8_t payload[FRAME_COLUMN_PAYLOAD_SIZE]; // Must be word aligned for DMA
    payload[0] = col & 0xFF; // column number (two bytes little endian)
    payload[1] = (col >> 8) & 0xFF;
    memcpy(&payload[2], rgb, LEDS_PER_WING * CHANNELS); // copy columns into payload

    return send_command(CMD_ANIM_COLUMN, payload, sizeof(payload));
}

bool spi_link_animation_end(void) {
    ESP_LOGI(TAG, "animation upload finished");
    return send_command(CMD_ANIM_END, NULL, 0);
}

bool spi_link_ping(void) {
    // Send ping transaction and check for error
    uint8_t resp;
    if (!spi_link_transact(CMD_PING, &resp, 1)) {
        return false;
    }

    // Confirm PONG is received
    bool ok = (resp == CMD_PING);
    if (ok) {
        ESP_LOGI(TAG, "PING -> PONG");
    } else {
        ESP_LOGW(TAG, "PING failed - got 0x%02x", resp);
    }
    return ok;
}

bool spi_link_push_column(uint16_t col, const uint8_t rgb[LEDS_PER_WING][CHANNELS]) {
    WORD_ALIGNED_ATTR uint8_t payload[FRAME_COLUMN_PAYLOAD_SIZE]; // Must be word aligned for DMA
    payload[0] = col & 0xFF; // Lower byte
    payload[1] = (col >> 8) & 0xFF; // Upper byte
    memcpy(&payload[2], rgb, LEDS_PER_WING * CHANNELS);

    return send_command(CMD_PUSH_COLUMN, payload, sizeof(payload));
}

bool spi_link_push_frame(const uint8_t frame[NUM_COLUMNS][LEDS_PER_WING][CHANNELS]) {
    for (int col = 0; col < NUM_COLUMNS; col++) {
        if (!spi_link_push_column(col, frame[col])) {
            ESP_LOGE(TAG, "push_frame aborted at column %d", col);
            return false;
        }
    }
    ESP_LOGI(TAG, "pushed full frame (%d columns)", NUM_COLUMNS);
    return true;
}

bool spi_link_set_calibration(float r_scale, float g_scale, float b_scale, float gamma, float radial_gamma, float max_duty_fraction, bool dither_enabled, float bcm_bits, float revs_per_frame) {
    WORD_ALIGNED_ATTR calibration_payload_t payload = { r_scale, g_scale, b_scale, gamma, radial_gamma, max_duty_fraction, dither_enabled ? 1.0f : 0.0f, bcm_bits, revs_per_frame }; // Must be word aligned for DMA

    return send_command(CMD_SET_CALIBRATION, &payload, sizeof(payload));
}

bool spi_link_get_calibration(calibration_payload_t *out) {
    uint8_t resp[sizeof(calibration_payload_t)];

    // Send transaction and check for error
    if (!spi_link_transact(CMD_GET_CALIBRATION, resp, sizeof(resp))) {
        return false;
    }

    // Copy response bytes across types (raw bytes back to calibration_payload_t type)
    memcpy(out, resp, sizeof(calibration_payload_t));
    return true;
}

bool spi_link_show_test_pattern(uint8_t pattern_id) {
    WORD_ALIGNED_ATTR uint8_t payload = pattern_id; // Must be word aligned for DMA

    return send_command(CMD_SHOW_TEST_PATTERN, &payload, TEST_PATTERN_PAYLOAD_SIZE);
}

// Note the return value is whether successful, and output is written to the pointer given as argument
bool spi_link_get_status(spi_status_t *out) {
    uint8_t resp[sizeof(spi_status_t)]; // Byte array correctly sized

    // Send transaction and check for error
    if (!spi_link_transact(CMD_GET_STATUS, resp, sizeof(resp))) {
        return false;
    }

    // Copy response bytes across types (raw bytes back to spi_status_t type)
    memcpy(out, resp, sizeof(spi_status_t));
    // Log response
    ESP_LOGI(TAG, "status: rpm=%.1f uptime=%lums",
             out->rpm, (unsigned long)out->uptime_ms);
    return true;
}
