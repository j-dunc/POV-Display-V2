// led_map: maps each LED's spatial position (led, channel) to its physical
// (chip, pin) coordinate on the LED driver chips

#include "led_map.h"

/*
Array holds the mapping of each LED channel to chain position (as if chips daisy were daisy-chained)
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

led_coord led_map[LEDS_PER_WING][CHANNELS];

// Build the full LED map for the wing based on the room map
void build_led_map(void) {
    for (int room = 0; room < LEDS_PER_WING/LEDS_PER_ROOM; room++) {
        for (int led = 0; led < LEDS_PER_ROOM; led++) {
            for (int chan = 0; chan < CHANNELS; chan++) {
                uint8_t chain_pos = room_map[led][chan] + (room*LEDS_PER_ROOM*CHANNELS);
                led_map[led+(room*LEDS_PER_ROOM)][chan].chip = chain_pos / LEDS_PER_ROOM;
                led_map[led+(room*LEDS_PER_ROOM)][chan].pin  = chain_pos % LEDS_PER_ROOM;
            }
        }
    }
}
