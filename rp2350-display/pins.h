#pragma once

// RP2350 pin map

// LHS driver data lines, chip 0 (centre) -> chip 8 (edge).
static const unsigned int LHS_DATA_PINS[9] = {10, 8, 7, 6, 4, 3, 2, 1, 0};

// RHS driver data lines, chip 0 (centre) -> chip 8 (edge). Contiguous.
static const unsigned int RHS_DATA_PINS[9] = {21, 22, 23, 24, 25, 26, 27, 28, 29};

#define PIN_HE_1          5   // Hall sensor 1, LEFT wing tip
#define PIN_OE            9   // Output enable, shared both wings, active LOW
#define PIN_LHS_CLK       11
#define PIN_RP_SPI_MOSI   12  // hardware SPI1 peripheral pin
#define PIN_RP_SPI_CSN    13  // hardware SPI1 peripheral pin
#define PIN_RP_SPI_SCK    14  // hardware SPI1 peripheral pin
#define PIN_RP_SPI_MISO   15  // hardware SPI1 peripheral pin
#define PIN_RP_ESP32_GPIO 16  // ready line to ESP32 for SPI link
#define PIN_RHS_CLK       18
#define PIN_LE            19  // Latch enable, shared both wings, active HIGH
#define PIN_HE_0          20  // Hall sensor 0, RIGHT wing tip
