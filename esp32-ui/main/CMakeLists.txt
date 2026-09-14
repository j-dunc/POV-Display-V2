idf_component_register(SRCS "main.c" "spi_link.c" "wifi_ap.c" "web_server.c" "test_pattern.c"
                    INCLUDE_DIRS "." "../../shared"
                    EMBED_TXTFILES "converter.html"
                    REQUIRES esp_driver_spi esp_driver_gpio log
                             esp_wifi esp_event nvs_flash esp_netif esp_http_server mdns)
