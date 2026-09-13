#pragma once

// Display geometry - single source of truth for RP2350 and ESP32

#define NUM_COLUMNS 298 // Gives square pixels on outer ring
#define DEG_PER_COLUMN (360.0f / NUM_COLUMNS)

#define LEDS_PER_WING 48
#define CHANNELS 3

#define RED   0
#define GREEN 1
#define BLUE  2
