#include "spi_link.h"
#include "pins.h"

#include "driver/spi_master.h"
#include "esp_log.h"


static const char *TAG = "spi_link";
static spi_device_handle_t spi_dev;

#define SPI_LINK_HOST     SPI2_HOST
#define SPI_LINK_CLOCK_HZ (1 * 1000 * 1000)

void spi_link_init(void) {
    // SPI bus config and init
    spi_bus_config_t bus_cfg = {
        .miso_io_num = PIN_SPI_MISO,
        .mosi_io_num = PIN_SPI_MOSI,
        .sclk_io_num = PIN_SPI_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 64
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI_LINK_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    // SPI device config and init
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = SPI_LINK_CLOCK_HZ,
        .mode = 0,
        .spics_io_num = PIN_SPI_CS,
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI_LINK_HOST, &dev_cfg, &spi_dev));
}

bool spi_link_echo_test_check(void) {
    uint8_t tx = 0x00;
    uint8_t rx = 0x00;
    spi_transaction_t t = {
        .length = 8, // bits
        .tx_buffer = &tx,
        .rx_buffer = &rx,
    };

    // Queue SPI transaction
    esp_err_t err = spi_device_transmit(spi_dev, &t);

    // Check if SPI failed
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI transaction failed: %s", esp_err_to_name(err));
        return false;
    }

    // Check if correct byte recieved
    bool ok = (rx == 0xA5);
    if (ok) {
        ESP_LOGI(TAG, "link OK (got 0x%02x)", rx);
    } else {
        ESP_LOGW(TAG, "link check FAILED - expected 0x%02x, got 0x%02x", 0xA5, rx);
    }
    return ok;
}
