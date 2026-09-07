#include "hall_sensor.h"
#include "pins.h"

#include <stdbool.h>
#include <stdio.h>
#include "hardware/gpio.h"
#include "pico/stdlib.h"

// Setup log - an array of structs containing timestamp, period, sensor
#define LOG_CAPACITY 2000

typedef struct {
    uint64_t timestamp_us;
    uint32_t period_us;
    uint8_t sensor;
} log_entry_t;

static log_entry_t log_buf[LOG_CAPACITY];
static volatile int log_count = 0;

// Note these must be voltatile as they are modified outside of where they're read so must be accessed from memory
// Both default at 0 until measured
// 0 index is HE sensor 0, 1 index is HE sensor 1
static volatile uint64_t last_timestamp_us[2] = {0};
static volatile uint32_t last_period_us[2] = {0};
static volatile bool has_triggered[2] = {false};

static void hall_irq_handler(uint gpio, uint32_t events) {
    uint64_t now = time_us_64();
    int sensor = (gpio == PIN_HE_0) ? 0 : 1; // Which sensor triggered?

    if (has_triggered[sensor]) {  // Only calculate period once we have two values
        uint32_t period = (uint32_t)(now - last_timestamp_us[sensor]); // Calculate difference between now and last trigger
        last_period_us[sensor] = period;

        // Log timestamp, period, sensor
        if (log_count < LOG_CAPACITY) {
            log_buf[log_count].timestamp_us = now;
            log_buf[log_count].period_us = period;
            log_buf[log_count].sensor = (uint8_t)sensor;
            log_count++;
        }
    }
    has_triggered[sensor] = true;
    last_timestamp_us[sensor] = now;
}

void hall_sensor_init(void) {
    // Register shared GPIO IRQ callback
    gpio_set_irq_enabled_with_callback(PIN_HE_0, GPIO_IRQ_EDGE_FALL, true, &hall_irq_handler);
    gpio_set_irq_enabled(PIN_HE_1, GPIO_IRQ_EDGE_FALL, true);
}

uint32_t hall_sensor_get_period_us(int sensor) {
    return last_period_us[sensor];
}

float hall_sensor_get_rpm(int sensor) {
    uint32_t period = last_period_us[sensor];
    if (period == 0) return 0.0f; // Return 0 if not measured yet
    return 60000000.0f / (float)period; // RPS = 1/T. RPM = 60 x 1/T. If T is in us, RPM = 60E6 /T
}


void hall_sensor_dump_log(void) {
    printf("--- hall sensor log (%d entries) ---\n", log_count);
    for (int i = 0; i < log_count; i++) {
        float rpm = 60000000.0f / (float)log_buf[i].period_us;
        printf("%d: sensor=%s t=%lu us period=%lu us rpm=%.1f\n",
               i,
               log_buf[i].sensor == 0 ? "HE_0" : "HE_1",
               (unsigned long)(uint32_t)log_buf[i].timestamp_us,
               (unsigned long)log_buf[i].period_us,
               rpm);
    }
    printf("--- end of log ---\n");
}
