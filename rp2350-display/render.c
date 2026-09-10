#include "render.h"
#include "image_source.h"
#include "hall_sensor.h"
#include "led_shift.h"
#include "pins.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"

// Blank below this RPM
#define MIN_READABLE_RPM 100.0f

// Fraction of each plane's weighted on-time the LEDs are actually driven for.
// Caps average current draw instead of a separate PWM/OE layer.
#define MAX_DUTY_FRACTION 0.5f

// Which RHS column is currently on display, -1 if display is blanked
static int last_col_rhs = -1;

/*
Shows all BCM_BITS planes of one column in sequence, each for a time
proportional to its binary weight (plane p is weight 2^p out of BCM_MAX_LEVEL),
so the eye blends the sequence into BCM_MAX_LEVEL+1 apparent brightness steps.
Blocks for the whole column_dwell_us
*/
static void show_column_bcm(int col_lhs, int col_rhs, uint32_t column_dwell_us) {
    for (int plane = 0; plane < BCM_BITS; plane++) {
        led_shift_send(image_lhs_words[col_lhs][plane], image_rhs_words[col_rhs][plane]);

        uint32_t weight = 1u << plane; // plane's weight in the level, e.g. plane 5 = bit 5 = 32
        uint32_t slot_us = (column_dwell_us * weight) / BCM_MAX_LEVEL;
        uint32_t on_us = (uint32_t)(slot_us * MAX_DUTY_FRACTION); // Scale all slots by the same factor keeps the ratio so only affects overall brightness

        gpio_put(PIN_OE, 0);
        sleep_us(on_us);
        gpio_put(PIN_OE, 1);
        sleep_us(slot_us - on_us);
    }
}

void render_init(void) {
    image_source_build();
}

void render_update(void) {
    uint64_t now = time_us_64();
    float angle = hall_sensor_get_angle_deg(now); // 0 deg = HE_0 = RHS wing tip
    float rpm = hall_sensor_get_rpm();

    // Blank if invalid state - no angle measured or rpm too low
    if (angle < 0.0f || rpm < MIN_READABLE_RPM) {
        gpio_put(PIN_OE, 1);
        last_col_rhs = -1;   // force a fresh column once valid again
        return;
    }

    // Calculate current column:
    // angle/deg_per_column gives how many columns into circle
    // int floors this
    // % num_columns ensures wraparound if it exceeds num_columns
    int col_rhs = (int)(angle / DEG_PER_COLUMN) % NUM_COLUMNS;
    // LHS wing always 180 deg ahead = NUM_COLUMNS/2 ahead
    int col_lhs = (col_rhs + NUM_COLUMNS / 2) % NUM_COLUMNS;

    // Start new column
    if (col_rhs != last_col_rhs) {
        uint32_t column_dwell_us = hall_sensor_get_period_us() / NUM_COLUMNS; // Time for this column
        show_column_bcm(col_lhs, col_rhs, column_dwell_us);
        last_col_rhs = col_rhs;
    }
}
