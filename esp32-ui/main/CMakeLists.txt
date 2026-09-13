idf_component_register(SRCS "main.c" "spi_link.c"
                    INCLUDE_DIRS "." "../../shared"
                    REQUIRES esp_driver_spi esp_driver_gpio log)
