idf_component_register(SRCS "main.c" "spi_link.c" "wifi_ap.c" "web_server.c"
                    INCLUDE_DIRS "." "../../shared"
                    REQUIRES esp_driver_spi esp_driver_gpio log
                             esp_wifi esp_event nvs_flash esp_netif esp_http_server mdns)
