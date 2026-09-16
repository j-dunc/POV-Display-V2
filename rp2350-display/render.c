#include "render.h"
#include "image_source.h"
#include "hall_sensor.h"
#include "led_shift.h"
#include "pins.h"

#include "pico/stdlib.h"
#include "pico/platform.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "RP2350.h"

// Fraction of each plane's weighted on-time the LEDs are actually driven for.
// Caps average current draw instead of a separate PWM/OE layer.
// Tuned live via SPI with CMD_SET_CALIBRATION
static float max_duty_fraction = 0.2f;

void render_set_max_duty_fraction(float fraction) {
    max_duty_fraction = fraction;
}

float render_get_max_duty_fraction(void) {
    return max_duty_fraction;
}

// Bench testing aid e.g. for logic analyser use
// Simulates hall effect rpm, period, and angle measurements
#define SIMULATE_HALL 0 // 1 = simulate, 0 = use hall effect sensors
#define SIM_RPM 900.0f // matches measured rate

// Which RHS column is currently on display, -1 if display is blanked
static int last_col_rhs = -1;

// Absolute cycle at which the next column is scheduled to start
static uint32_t next_column_start_cycle;

// System clock frequency in Hz
// Needed to busy wait in cycles instead of using hardware timer which has 1 us resolution - too small
static uint32_t clk_hz;

#if SIMULATE_HALL
static uint64_t sim_start_us;
#endif

// Busy-waits until DWT->CYCCNT reaches (or has already passed) target_cycle
static inline void wait_until_cycle(uint32_t target_cycle) {
    while ((int32_t)(DWT->CYCCNT - target_cycle) < 0) {
        tight_loop_contents();
    }
}

void render_init(void) {
    clk_hz = clock_get_hz(clk_sys);

    // Enable the DWT free-running cycle counter so show_column_bcm can measure
    // its own real elapsed time including overheads
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // Enable tracing
    DWT->CYCCNT = 0; // Reset cycle counter
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk; // Enable cycle counter

#if SIMULATE_HALL
    sim_start_us = time_us_64();
#endif

    image_source_build_palette(); // holds each colour for ~50 columns - easier to scope-check a channel's BCM shape than the rainbow's constantly-shifting hue
}

/*
Shows all active BCM bit-planes of one column
twice, each pass at half weight, in a mirrored (palindrome) plane order e.g.
5 4 3 2 1 0 1 2 3 4 5 so the centroid is at the centre of the pixel
The exception is the weight-1 plane, which is shown once, full weight
to ensure there is time for shift and latch overhead during off period
Descending then ascending to split largest planes as much as possible

Each plane's on-time (on_cycles) is fixed and exact, since it's what sets displayed
brightness.
Each plane's off-time is worked out from DWT cycle counter, to self-correct for overhead
in calls rather than stack up these overheads across each plane, overrunning the column end

column_start_cycle is when this column was scheduled to start so any extra column overhead
gets absorbed into off-time
*/
static void show_column_bcm(int col_lhs, int col_rhs, uint32_t column_dwell_cycles, uint32_t column_start_cycle) {
    uint32_t cumulative_cycle = column_start_cycle; // Absolute cycle count marking the end of the current plane's slot
    uint32_t column_deadline_cycle = column_start_cycle + column_dwell_cycles; // absolute cycle the column must not run past

    int bcm_bits = image_source_get_bcm_bits();
    int bcm_max_level = (1 << bcm_bits) - 1;

    for (int half = 0; half < 2; half++) {
        for (int i = 0; i < bcm_bits; i++) {
            int plane = (half == 0) ? (bcm_bits - 1 - i) : i; // descending then ascending

            if (half == 1 && plane == 0) continue; // smallest plane already shown once, full weight, at the end of half 0 so do not repeat

            uint32_t weight = 1u << plane; // plane's weight in the level, e.g. plane 5 = bit 5 = 32
            uint32_t slot_cycles = column_dwell_cycles * weight / bcm_max_level / ((plane == 0) ? 1 : 2); // weight according to plane weight, and half except for smallest plane
            uint32_t on_cycles = (uint32_t)(slot_cycles * max_duty_fraction); // Scale all slots by the same factor keeps the ratio so only affects overall brightness
            cumulative_cycle += slot_cycles;
            uint32_t on_start_cycle = cumulative_cycle - on_cycles; // when this plane's on-time should begin

            // OE already high here - start this plane's shift and let it run
            // in the background while we spend the blank time needed anyway for duty cycle
            led_shift_start(image_lhs_words[col_lhs][plane], image_rhs_words[col_rhs][plane]);

            // Wait until this plane's scheduled on-time, based on real elapsed time
            wait_until_cycle(on_start_cycle);

            led_shift_finish(); // blocks only if the shift is somehow not done yet, so garbage isnt latched
            led_shift_latch();

            gpio_put(PIN_OE, 0);
            // Measured from the actual on phase start (not the schedule), so this plane always
            // gets exactly on_cycles of real on-time regardless of any overrun
            // Self correcting for overruns occurs only in the off phases
            uint32_t on_target_cycle = DWT->CYCCNT + on_cycles;
            // Clamp to the column deadline so columns never overrun, so columns are never skipped in a revolution
            if ((int32_t)(on_target_cycle - column_deadline_cycle) > 0) on_target_cycle = column_deadline_cycle;
            wait_until_cycle(on_target_cycle);
            gpio_put(PIN_OE, 1);
        }
    }
}

void render_update(void) {
    uint64_t now = time_us_64();

#if SIMULATE_HALL
    // Synthetic angle/period in place of the real hall sensors
    uint32_t period_us = (uint32_t)(60000000.0f / SIM_RPM);
    float angle = (float)((now - sim_start_us) % period_us) * (360.0f / (float)period_us);
    float rpm = SIM_RPM;
#else
    float angle = hall_sensor_get_angle_deg(now); // 0 deg = HE_0 = RHS wing tip
    float rpm = hall_sensor_get_rpm();
#endif

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
#if SIMULATE_HALL
        uint32_t revolution_us = period_us;
#else
        uint32_t revolution_us = hall_sensor_get_period_us();
#endif
        // Divide into columns in cycles rather than whole microseconds
        uint32_t revolution_cycles = (uint32_t)(((uint64_t)revolution_us * clk_hz) / 1000000); // Intermediate must cast to 64 bit to avoid overflow
        uint32_t column_dwell_cycles = revolution_cycles / NUM_COLUMNS;

        // Re-anchor the grid to the real shaft position if necessary
        uint32_t now_cycle = DWT->CYCCNT;
        int32_t schedule_error = (int32_t)(now_cycle - next_column_start_cycle);
        if (last_col_rhs < 0 // Display blanked - invalid RPM or startup
            || col_rhs < last_col_rhs  // Full revolution completed             
            || schedule_error > (int32_t)column_dwell_cycles // Grid has fallen more than a column behind schedule             
            || schedule_error < -(int32_t)column_dwell_cycles) { // Grid is ahead by more than a column
            next_column_start_cycle = now_cycle;
        }

        show_column_bcm(col_lhs, col_rhs, column_dwell_cycles, next_column_start_cycle);
        next_column_start_cycle += column_dwell_cycles; // fixed grid - don't re-read from the clock
        last_col_rhs = col_rhs;
    }
}
