#pragma once

#include <stdint.h>

#define LEDS_PER_WING 48
#define CHANNELS 3
#define LEDS_PER_ROOM 16

#define RED 0
#define GREEN 1
#define BLUE 2


// Store led position as chip,pin coordinates
typedef struct {
    uint8_t chip;
    uint8_t pin;
} led_coord;

// Populates LED map, must be called before led map is used
void build_led_map(void);

/*
Maps each led's spatial position and channel to chip,pin coordinates
Extern so can be shared by files without duplicating
*/
extern led_coord led_map[LEDS_PER_WING][CHANNELS];
