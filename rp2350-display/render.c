#include "render.h"
#include "image_source.h"
#include "hall_sensor.h"
#include "led_shift.h"
#include "pins.h"

#include "pico/stdlib.h"
#include "pico/platform.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"

// Blank below this RPM
#define MIN_READABLE_RPM 100.0f

// Fraction of each plane's weighted on-time the LEDs are actually driven for.
// Caps average current draw instead of a separate PWM/OE layer.
#define MAX_DUTY_FRACTION 0.5f

// Which RHS column is currently on display, -1 if display is blanked
static int last_col_rhs = -1;

// System clock frequency in Hz
// Needed to busy wait in cycles instead of using hardware timer which has 1 us resolution - too small
static uint32_t clk_hz;

void render_init(void) {
    clk_hz = clock_get_hz(clk_sys);
    image_source_build_rainbow();
}

/*
Shows all BCM_BITS planes of one column twice, each pass at half weight, in a
mirrored (palindrome) plane order e.g. 5 4 3 2 1 0 0 1 2 3 4 5
so the centroid is at the centre of the pixel
Descending then ascending to split largest planes as much as possible

Each plane's blank (off) period comes before its on period rather than after -
OE is always already high on entry (board_init sets it, and every exit path of
this loop/render_update leaves it that way), so the next plane's shift can be
kicked off right away and run in the background (via DMA/PIO in hardware) while
the CPU busy-waits out off_cycles. led_shift_finish() then only has to block on
whatever's left, which is normally nothing - so the shift is effectively free
with no calibrated overhead constant needed, unless a plane's off_cycles is
ever shorter than the shift itself, in which case that one blank period simply
stretches slightly to cover it.
Blocks for the whole column_dwell_us
*/
static void show_column_bcm(int col_lhs, int col_rhs, uint32_t column_dwell_us) {
    uint64_t column_dwell_cycles = ((uint64_t)column_dwell_us * clk_hz) / 1000000;

    for (int half = 0; half < 2; half++) {
        for (int i = 0; i < BCM_BITS; i++) {
            int plane = (half == 0) ? (BCM_BITS - 1 - i) : i; // descending then ascending

            uint32_t weight = 1u << plane; // plane's weight in the level, e.g. plane 5 = bit 5 = 32
            uint32_t slot_cycles = (uint32_t)((column_dwell_cycles * weight) / BCM_MAX_LEVEL / 2); // halved as shown twice for each column
            uint32_t on_cycles = (uint32_t)(slot_cycles * MAX_DUTY_FRACTION); // Scale all slots by the same factor keeps the ratio so only affects overall brightness
            uint32_t off_cycles = slot_cycles - on_cycles;

            // OE already high here - start this plane's shift and let it run
            // in the background while we spend the blank time needed anyway for duty cycle
            led_shift_start(image_lhs_words[col_lhs][plane], image_rhs_words[col_rhs][plane]);
            busy_wait_at_least_cycles(off_cycles);
            led_shift_finish(); // blocks only if the shift is somehow not done yet
            led_shift_latch();

            gpio_put(PIN_OE, 0);
            busy_wait_at_least_cycles(on_cycles);
            gpio_put(PIN_OE, 1);
        }
    }
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
