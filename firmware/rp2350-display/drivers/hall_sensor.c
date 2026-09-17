// hall_sensor: reads the two hall-effect sensors and derives shaft angle,
// RPM, and revolution period from their edge timing

#include "hall_sensor.h"
#include "pins.h"

#include <math.h>
#include <stdbool.h>
#include "hardware/gpio.h"
#include "pico/stdlib.h"

// Bench testing aid e.g. for testing over USB with no motor spinning
#define SIMULATE_HALL 0 // 1 = simulate, 0 = use hall effect sensors
#define SIM_RPM 1250.0f // matches max RPM

#if SIMULATE_HALL

static uint64_t sim_start_us;

void hall_sensor_init(void) {
    sim_start_us = time_us_64();
}

uint32_t hall_sensor_get_period_us(void) {
    return (uint32_t)(60000000.0f / SIM_RPM);
}

float hall_sensor_get_angle_deg(uint64_t now_us) {
    uint32_t period_us = hall_sensor_get_period_us();
    return (float)((now_us - sim_start_us) % period_us) / (float)period_us * 360.0f;
}

#else

static volatile bool has_any_trigger = false; // Have any sensors been triggered yet
static volatile int last_edge_sensor = -1; // Which sensor triggered most recently - -1 if unknown
static volatile uint64_t last_edge_timestamp_us = 0; // of either sensor
static volatile uint32_t last_half_period_us = 0; // Time since the other sensor last triggered

// Debounce floor - real half periods are longer at spinning RPMs
#define MIN_EDGE_INTERVAL_US 1000

static void hall_irq_handler(uint gpio, uint32_t events) {
    uint64_t now = time_us_64();

    // Debounce
    if (has_any_trigger && (now - last_edge_timestamp_us) < MIN_EDGE_INTERVAL_US) return;

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
    // Sensors are open-collector, so need a pull-up to read a valid level
    gpio_init(PIN_HE_0);
    gpio_set_dir(PIN_HE_0, GPIO_IN);
    gpio_pull_up(PIN_HE_0);
    gpio_init(PIN_HE_1);
    gpio_set_dir(PIN_HE_1, GPIO_IN);
    gpio_pull_up(PIN_HE_1);

    // Register shared GPIO IRQ callback
    gpio_set_irq_enabled_with_callback(PIN_HE_0, GPIO_IRQ_EDGE_FALL, true, &hall_irq_handler);
    gpio_set_irq_enabled(PIN_HE_1, GPIO_IRQ_EDGE_FALL, true);
}

float hall_sensor_get_angle_deg(uint64_t now_us) {
    // Return -1 if triggers not measured yet
    if (!has_any_trigger || last_half_period_us == 0) return -1.0f;

    uint64_t elapsed = now_us - last_edge_timestamp_us;

    // Stop extrapolating from a stale timestamp if stalled / hall missed etc.
    if (elapsed > (uint64_t)last_half_period_us * 4) return -1.0f;

    // HE_0 defines 0 degrees, HE_1 defines 180 degrees
    float last_edge_angle_deg = (last_edge_sensor == 0) ? 0.0f : 180.0f;

    // Assume constant angular velocity - calculated from half period
    // Use angular velocity, time elapsed, and starting angle to calculate current angle
    float rate_deg_per_us = 180.0f / (float)last_half_period_us;
    float angle = last_edge_angle_deg + rate_deg_per_us * (float)elapsed;

    return fmodf(angle, 360.0f); // Ensure between 0 and 360
}

uint32_t hall_sensor_get_period_us(void) {
    if (!has_any_trigger || last_half_period_us == 0) return 0; // Return 0 if no period measured

    // Stop reporting a stale period once stopped / hall missed etc, same cutoff as hall_sensor_get_angle_deg
    uint64_t elapsed = time_us_64() - last_edge_timestamp_us;
    if (elapsed > (uint64_t)last_half_period_us * 4) return 0;

    return last_half_period_us * 2; // half-revolution -> full revolution
}

#endif

float hall_sensor_get_rpm(void) {
    uint32_t period = hall_sensor_get_period_us();
    if (period == 0) return 0.0f;
    return 60000000.0f / (float)period; // RPS = 1/T. RPM = 60 x 1/T. If T is in us, RPM = 60E6 /T
}
