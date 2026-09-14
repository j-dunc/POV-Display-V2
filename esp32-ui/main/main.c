#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdbool.h>

#include "nvs_flash.h"

#include "spi_link.h"
#include "wifi_ap.h"
#include "web_server.h"
#include "test_pattern.h"

static uint8_t test_frame[NUM_COLUMNS][LEDS_PER_WING][CHANNELS];

void app_main(void)
{
    // WiFi needs NVS to store calibration data
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_ap_init();
    web_server_start();

    spi_link_init();

    // Test: push solid red/blue, alternating every 5 seconds
    uint8_t red[CHANNELS] = {255, 0, 0};
    uint8_t blue[CHANNELS] = {0, 0, 255};
    bool showing_red = true;
    int seconds_since_push = 0;

    test_pattern_build(test_frame, red);
    spi_link_push_frame(test_frame);

    while (true) {
        spi_link_ping();

        spi_status_t status;
        spi_link_get_status(&status);

        vTaskDelay(pdMS_TO_TICKS(1000)); // Wait 1 second without blocking other tasks

        if (++seconds_since_push >= 5) {
            seconds_since_push = 0;
            showing_red = !showing_red;
            test_pattern_build(test_frame, showing_red ? red : blue);
            spi_link_push_frame(test_frame);
        }
    }
}
