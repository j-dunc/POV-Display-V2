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
#define SPI_LINK_CLOCK_HZ (1 * 1000 * 1000)

// How long to wait for the RP2350's ready line before giving up on a command
#define READY_TIMEOUT_MS 100


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
static bool wait_ready(void) {
    return xSemaphoreTake(ready_sem, pdMS_TO_TICKS(READY_TIMEOUT_MS)) == pdTRUE;
}

/*
Every command is two SPI transactions: send the command byte, wait for the
RP2350's ready line, then clock the reply back out.
*/
static bool spi_link_transact(uint8_t cmd, uint8_t *resp, size_t resp_len) {
    // Wait for RP2350 ready
    if (!wait_ready()) {
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
    if (!wait_ready()) {
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
    // Wait for RP2350 ready, then send the command byte (no rx)
    if (!wait_ready()) {
        ESP_LOGW(TAG, "RP2350 not ready for PUSH_COLUMN command");
        return false;
    }
    uint8_t cmd = CMD_PUSH_COLUMN;
    spi_transaction_t cmd_t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    esp_err_t err = spi_device_transmit(spi_dev, &cmd_t);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "PUSH_COLUMN command transaction failed: %s", esp_err_to_name(err));
        return false;
    }

    // Wait for RP2350 ready
    if (!wait_ready()) {
        ESP_LOGW(TAG, "RP2350 not ready for column %u payload", col);
        return false;
    }
    // Build payload - column index then rgb data
    WORD_ALIGNED_ATTR uint8_t payload[FRAME_COLUMN_PAYLOAD_SIZE]; // Must be word aligned for DMA
    payload[0] = col & 0xFF; // Lower byte
    payload[1] = (col >> 8) & 0xFF; // Upper byte
    memcpy(&payload[2], rgb, LEDS_PER_WING * CHANNELS); 

    spi_transaction_t payload_t = {
        .length = sizeof(payload) * 8,
        .tx_buffer = payload,
    };
    // Send transaction (no rx expected back) and check for errors
    err = spi_device_transmit(spi_dev, &payload_t);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "column %u payload transaction failed: %s", col, esp_err_to_name(err));
        return false;
    }

    return true;
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

bool spi_link_set_calibration(float r_scale, float g_scale, float b_scale, float gamma, float radial_gamma, float max_duty_fraction, bool dither_enabled, float bcm_bits) {
    // Wait for RP2350 ready, then send the command byte (no rx)
    if (!wait_ready()) {
        ESP_LOGW(TAG, "RP2350 not ready for SET_CALIBRATION command");
        return false;
    }
    uint8_t cmd = CMD_SET_CALIBRATION;
    spi_transaction_t cmd_t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    esp_err_t err = spi_device_transmit(spi_dev, &cmd_t);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SET_CALIBRATION command transaction failed: %s", esp_err_to_name(err));
        return false;
    }

    // Wait for RP2350 ready
    if (!wait_ready()) {
        ESP_LOGW(TAG, "RP2350 not ready for calibration payload");
        return false;
    }
    WORD_ALIGNED_ATTR calibration_payload_t payload = { r_scale, g_scale, b_scale, gamma, radial_gamma, max_duty_fraction, dither_enabled ? 1.0f : 0.0f, bcm_bits }; // Must be word aligned for DMA
    spi_transaction_t payload_t = {
        .length = sizeof(payload) * 8,
        .tx_buffer = &payload,
    };
    // Send transaction (no rx expected back) and check for errors
    err = spi_device_transmit(spi_dev, &payload_t);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "calibration payload transaction failed: %s", esp_err_to_name(err));
        return false;
    }

    return true;
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
