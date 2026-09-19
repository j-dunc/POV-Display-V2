idf_component_register(SRCS "main.c" "spi_link.c" "wifi_ap.c" "web/web_server.c"
                    INCLUDE_DIRS "." "web" "../../shared"
                    EMBED_TXTFILES "web/converter.html" "web/calibrate.html" "web/status.js" "web/gif.js"
                    REQUIRES esp_driver_spi esp_driver_gpio log
                             esp_wifi esp_event nvs_flash esp_netif esp_http_server mdns)
