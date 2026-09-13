#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "spi_link.h"

void app_main(void)
{
    spi_link_init();

    while (true) {
        spi_link_ping();

        spi_status_t status;
        spi_link_get_status(&status);

        vTaskDelay(pdMS_TO_TICKS(1000)); // Wait 1 second without blocking other tasks
    }
}
