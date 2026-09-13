#pragma once

#include <stdint.h>

#include "frame_geometry.h"

#define LEDS_PER_ROOM 16

// Store led position as chip,pin coordinates
typedef struct {
    uint8_t chip;
    uint8_t pin;
} led_coord;

// Populates LED map, must be called before led map is used
void build_led_map(void);

/*
Maps each led's spatial position (0 to 47) and channel (0 to 2) to chip,pin coordinates ((0,0) to (8,15))
Extern so can be shared by files without duplicating
*/
extern led_coord led_map[LEDS_PER_WING][CHANNELS];
