#include "render.h"
#include "image_source.h"
#include "hall_sensor.h"
#include "led_shift.h"
#include "pins.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"

// Blank below this RPM
#define MIN_READABLE_RPM 100.0f

// Which RHS column is currently on display, -1 if display is blanked
static int last_col_rhs = -1;

void render_init(void) {
    image_source_build();
}

void render_update(void) {
    uint64_t now = time_us_64();
    float angle = hall_sensor_get_angle_deg(now); // 0 deg = HE_0 = RHS wing tip
    float rpm = hall_sensor_get_rpm();

    if (angle < 0.0f || rpm < MIN_READABLE_RPM) {
        gpio_put(PIN_OE, 1); // blank - no angle fix yet, or not spinning fast enough
        last_col_rhs = -1;   // force a fresh shift once valid again
        return;
    }

    // Calculate current column:
    // angle/deg_per_column gives how many columns into circle
    // int floors this
    // % num_columns ensures wraparound if it exceeds num_columns
    int col_rhs = (int)(angle / DEG_PER_COLUMN) % NUM_COLUMNS;
    // LHS wing always 180 deg ahead = NUM_COLUMNS/2 ahead
    int col_lhs = (col_rhs + NUM_COLUMNS / 2) % NUM_COLUMNS;

    // Shifts only occur on a new column
    if (col_rhs != last_col_rhs) {
        led_shift_send(image_lhs_words[col_lhs], image_rhs_words[col_rhs]);
        gpio_put(PIN_OE, 0); // Enables LED in case disabled earlier
        last_col_rhs = col_rhs;
    }
}
