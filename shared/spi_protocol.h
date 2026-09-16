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

// GET_STATUS response payload
typedef struct {
    float rpm;          // 0 if not currently measurable
    uint8_t spinning;   // 1 if rpm is above the render loop's readable threshold
    uint32_t uptime_ms;
} spi_status_t;

// PUSH_COLUMN payload: 2-byte column index (little-endian), followed by
// LEDS_PER_WING * CHANNELS bytes of raw 0-255 RGB
#define FRAME_COLUMN_PAYLOAD_SIZE (2 + LEDS_PER_WING * CHANNELS)

// SET_CALIBRATION payload: per-channel brightness scale , a shared gamma exponent, and a radial exponent
// Applied to every colour before it's converted to BCM. No reply is sent.
// GET_CALIBRATION has no payload; it replies with the current values in this same layout
typedef struct {
    float r_scale;
    float g_scale;
    float b_scale;
    float gamma;
    float radial_gamma;
} calibration_payload_t;
#define CALIBRATION_PAYLOAD_SIZE (sizeof(calibration_payload_t))
