#pragma once

#include <stdint.h>


// Set up interrupts
void hall_sensor_init(void);

// Most recent time between consecutive triggers of the given sensor
// 0 if the not yet measured
uint32_t hall_sensor_get_period_us(int sensor);

// RPM derived from period - 0 if not yet measured
float hall_sensor_get_rpm(int sensor);

// Prints log of all period, RPM measurements
void hall_sensor_dump_log(void);
