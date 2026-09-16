#pragma once

#include <stdint.h>


// Set up interrupts
void hall_sensor_init(void);

// Current angular position (given the current time) in degrees (0-360), interpolated from sensors
// -1 if not yet measured
float hall_sensor_get_angle_deg(uint64_t now_us);

// Period calculated from most recent half-period (time between HE0 and HE1)
// 0 if not yet measured, or if stalled
uint32_t hall_sensor_get_period_us(void);

// RPM derived from period, 0 if not yet measured or stalled
float hall_sensor_get_rpm(void);
