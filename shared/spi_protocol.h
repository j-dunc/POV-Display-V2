#pragma once

#include <stdint.h>

#include "frame_geometry.h"

// RP2350 <-> ESP32 SPI protocol definitions, shared by both

// Each command is two transfers
// ESP32 sends CMD, waits for RP2350 ready line, then clocks back reply

// SPI Commands IDs
#define CMD_PING       0x01
#define CMD_GET_STATUS 0x02

// GET_STATUS response payload
typedef struct {
    float rpm;          // 0 if not currently measurable
    uint8_t spinning;   // 1 if rpm is above the render loop's readable threshold
    uint32_t uptime_ms;
} spi_status_t;
