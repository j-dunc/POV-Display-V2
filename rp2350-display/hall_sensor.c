#include "hall_sensor.h"
#include "pins.h"

#include <stdbool.h>
#include "hardware/gpio.h"
#include "pico/stdlib.h"

static volatile bool has_any_trigger = false; // Have any sensors been triggered yet
static volatile int last_edge_sensor = -1; // Which sensor triggered most recently - -1 if unknown
static volatile uint64_t last_edge_timestamp_us = 0; // of either sensor
static volatile uint32_t last_half_period_us = 0; // Time since the other sensor last triggered

static void hall_irq_handler(uint gpio, uint32_t events) {
    uint64_t now = time_us_64();
    int sensor = (gpio == PIN_HE_0) ? 0 : 1; // Which sensor triggered?

    // Track time since the other sensor triggered
    // Skip if we missed a trigger or if we don't have two values
    if (has_any_trigger && last_edge_sensor != sensor) {
        last_half_period_us = (uint32_t)(now - last_edge_timestamp_us);
    }
    has_any_trigger = true;
    last_edge_sensor = sensor;
    last_edge_timestamp_us = now;
}

void hall_sensor_init(void) {
    // Register shared GPIO IRQ callback
    gpio_set_irq_enabled_with_callback(PIN_HE_0, GPIO_IRQ_EDGE_FALL, true, &hall_irq_handler);
    gpio_set_irq_enabled(PIN_HE_1, GPIO_IRQ_EDGE_FALL, true);
}

float hall_sensor_get_angle_deg(uint64_t now_us) {
    // Return -1 if triggers not measured yet
    if (!has_any_trigger || last_half_period_us == 0) return -1.0f;

    // HE_0 defines 0 degrees, HE_1 defines 180 degrees
    float last_edge_angle_deg = (last_edge_sensor == 0) ? 0.0f : 180.0f;

    // Assume constant angular velocity - calculated from half period
    // Use angular velocity, time elapsed, and starting angle to calculate current angle
    uint64_t elapsed = now_us - last_edge_timestamp_us;
    float rate_deg_per_us = 180.0f / (float)last_half_period_us;
    float angle = last_edge_angle_deg + rate_deg_per_us * (float)elapsed;
    while (angle >= 360.0f) angle -= 360.0f; // Ensure between 0 and 360
    return angle;
}

uint32_t hall_sensor_get_period_us(void) {
    if (!has_any_trigger || last_half_period_us == 0) return 0; // Return 0 if no period measured
    return last_half_period_us * 2; // half-revolution -> full revolution
}

float hall_sensor_get_rpm(void) {
    uint32_t period = hall_sensor_get_period_us();
    if (period == 0) return 0.0f;
    return 60000000.0f / (float)period; // RPS = 1/T. RPM = 60 x 1/T. If T is in us, RPM = 60E6 /T
}
