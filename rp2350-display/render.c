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

// Blank below this RPM
#define MIN_READABLE_RPM 100.0f

// Fraction of each plane's weighted on-time the LEDs are actually driven for.
// Caps average current draw instead of a separate PWM/OE layer.
#define MAX_DUTY_FRACTION 0.5f

// Bench testing aid e.g. for logic analyser use
// Simulates hall effect rpm, period, and angle measurements
#define SIMULATE_HALL 1 // 1 = simulate, 0 = use hall effect sensors
#define SIM_RPM 900.0f // matches measured rate

// Which RHS column is currently on display, -1 if display is blanked
static int last_col_rhs = -1;

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

    image_source_build_rainbow(); // holds each colour for ~50 columns - easier to scope-check a channel's BCM shape than the rainbow's constantly-shifting hue
}

/*
Shows all BCM_BITS planes of one column twice, each pass at half weight, in a
mirrored (palindrome) plane order e.g. 5 4 3 2 1 0 1 2 3 4 5
so the centroid is at the centre of the pixel
The exception is the weight-1 plane, which is shown once, full weight
to ensure there is time for shift and latch overhead during off period
Descending then ascending to split largest planes as much as possible

Each plane's on-time (on_cycles) is fixed and exact, since it's what sets displayed
brightness.
Each plane's off-time is worked out from DWT cycle counter, to self-correct for overhead
in calls rather than stack up these overheads across each plane, overrunning the column end
*/
static void show_column_bcm(int col_lhs, int col_rhs, uint32_t column_dwell_us) {
    uint64_t column_dwell_cycles = ((uint64_t)column_dwell_us * clk_hz) / 1000000;
    uint32_t cumulative_cycle = DWT->CYCCNT; // Absolute cycle count marking the end of the current plane's slot

    for (int half = 0; half < 2; half++) {
        for (int i = 0; i < BCM_BITS; i++) {
            int plane = (half == 0) ? (BCM_BITS - 1 - i) : i; // descending then ascending

            if (half == 1 && plane == 0) continue; // smallest plane already shown once, full weight, at the end of half 0 so do not repeat

            uint32_t weight = 1u << plane; // plane's weight in the level, e.g. plane 5 = bit 5 = 32
            uint32_t slot_cycles = (uint32_t)(column_dwell_cycles * weight / BCM_MAX_LEVEL / ((plane == 0) ? 1 : 2)); // weight according to plane weight, and half except for smallest plane
            uint32_t on_cycles = (uint32_t)(slot_cycles * MAX_DUTY_FRACTION); // Scale all slots by the same factor keeps the ratio so only affects overall brightness
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
            wait_until_cycle(DWT->CYCCNT + on_cycles);
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
        uint32_t column_dwell_us = period_us / NUM_COLUMNS;
#else
        uint32_t column_dwell_us = hall_sensor_get_period_us() / NUM_COLUMNS; // Time for this column
#endif
        show_column_bcm(col_lhs, col_rhs, column_dwell_us);
        last_col_rhs = col_rhs;
    }
}
