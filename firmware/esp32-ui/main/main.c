// main: entry point. Starts the SoftAP + HTTP server,
// then initialises the SPI link to the RP2350

#include "nvs_flash.h"

#include "spi_link.h"
#include "wifi_ap.h"
#include "web_server.h"

void app_main(void)
{
    // WiFi needs NVS to store calibration data
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Setup wifi and server
    wifi_ap_init();
    web_server_start();

    // Setup SPI
    spi_link_init();

}
