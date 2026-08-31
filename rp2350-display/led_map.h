#pragma once

#include <stdint.h>

#define LEDS_PER_WING 48
#define CHANNELS 3
#define LEDS_PER_ROOM 16
#define BYTES_PER_WING ((LEDS_PER_WING * CHANNELS)/8)

#define RED 0
#define GREEN 1
#define BLUE 2

/*
Array holds the mapping of each LED channel to chain position
The pattern is repeated for each group of 3 chips, so the full map is built from this
The map is the same for each wing
*/

static const uint8_t room_map[LEDS_PER_ROOM][CHANNELS] = {
    {  0,  14,  15},  // led 0 R,G,B
    {  1,   4,   2},  // etc.
    {  3,  13,   6},  
    {  5,  11,  12},  
    {  7,   9,  10},  
    { 16,  31,   8}, 
    { 17,  19,  30}, 
    { 18,  29,  21}, 
    { 20,  27,  28}, 
    { 22,  25,  26}, 
    { 23,  47,  24}, 
    { 32,  33,  46},  
    { 34,  38,  36}, 
    { 35,  44,  45},  
    { 37,  42,  43}, 
    { 39,  40,  41}, // led 15 R,G,B
};

static uint8_t led_map[LEDS_PER_WING][CHANNELS];

// Build the full LED map for the wing based on the room map
static void build_led_map() {
    for (int i=0; i<LEDS_PER_WING/LEDS_PER_ROOM; i++) {
        for (int j=0; j<LEDS_PER_ROOM; j++) {
            for (int k=0; k<CHANNELS; k++) {
                led_map[j+(i*LEDS_PER_ROOM)][k] = room_map[j][k] + (i*LEDS_PER_ROOM*CHANNELS); 
            }
        }
    }
}