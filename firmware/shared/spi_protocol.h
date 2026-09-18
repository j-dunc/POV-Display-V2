#pragma once

#include <stdint.h>

#include "frame_geometry.h"

// RP2350 <-> ESP32 SPI protocol definitions, shared by both

// Each command is two transfers
// ESP32 sends CMD, waits for RP2350 ready line, then clocks reply back or data out

// SPI Commands IDs
#define CMD_PING           0x01
#define CMD_GET_STATUS     0x02
#define CMD_PUSH_COLUMN    0x03
#define CMD_SET_CALIBRATION 0x04
#define CMD_GET_CALIBRATION 0x05
#define CMD_SHOW_TEST_PATTERN 0x06
#define CMD_ANIM_BEGIN     0x07
#define CMD_ANIM_COLUMN    0x08
#define CMD_ANIM_END       0x09

// GET_STATUS response payload
typedef struct {
    float rpm;          // 0 if not currently measurable, or not spinning fast enough to read
    uint32_t uptime_ms;
} spi_status_t;

// PUSH_COLUMN payload: 2-byte column index (little-endian), followed by
// LEDS_PER_WING * CHANNELS bytes of raw 0-255 RGB
#define FRAME_COLUMN_PAYLOAD_SIZE (2 + LEDS_PER_WING * CHANNELS)

// SET_CALIBRATION payload: per-channel brightness scale, a shared gamma exponent, a radial
// exponent, and the overall max duty fraction
// Colour fields are applied to every colour before it's converted to BCM; max_duty_fraction
// scales every BCM plane's on-time. No reply is sent.
// GET_CALIBRATION has no payload; it replies with the current values in this same layout
typedef struct {
    float r_scale;
    float g_scale;
    float b_scale;
    float gamma;
    float radial_gamma;
    float max_duty_fraction;
    float dither_enabled; // 1.0 = on, 0.0 = off - float for uniform layout with the rest of this struct
    float bcm_bits; // BCM bit depth, 1-BCM_BITS_MAX (8) - float for uniform layout, rounded and clamped on receipt
} calibration_payload_t;
#define CALIBRATION_PAYLOAD_SIZE (sizeof(calibration_payload_t))


// ANIM_BEGIN payload: 2-byte frame count (little-endian), then 1 byte revs_per_frame
// Erases flash for frame count
// ANIM_COLUMN payload: same layout as PUSH_COLUMN
// Sends one column at a time in order
// ANIM_END has no payload
// Signals to play animation
// None reply
#define ANIM_BEGIN_PAYLOAD_SIZE 3
// frame stride approx 43kB
// reserved flash = 15MB
#define MAX_ANIMATION_FRAMES 300  

// SHOW_TEST_PATTERN payload: 1-byte pattern ID, one of TEST_PATTERN_*. No reply.
#define TEST_PATTERN_PAYLOAD_SIZE 1
#define TEST_PATTERN_RED_RAMP    0 // angular ramp, red only, 0 to full scale once per revolution
#define TEST_PATTERN_GREEN_RAMP  1 // green ramp
#define TEST_PATTERN_BLUE_RAMP   2 // blue ramp
#define TEST_PATTERN_GRAY_RAMP   3 // all channels (grey) ramp
#define TEST_PATTERN_RADIAL_WHITE 4 // solid white
#define TEST_PATTERN_RAINBOW     5 // smooth hue sweep, one full turn per revolution
