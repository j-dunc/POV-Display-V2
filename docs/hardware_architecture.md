# POV Display — Hardware Architecture (V2)
Spinning PCB persistence-of-vision display, two wings of RGB LEDs
(LHS/RHS) forming a circular image as the board rotates (CW) on a BLDC motor.
Wireless power to the spinning board. This document describes the
schematic/PCB as designed — pin assignments, chip topology, and
electrical facts.
## Chips

- **RP2350A** — main render MCU.
- **ESP32-C3-MINI-1-H4** — Wi-Fi companion, connected to RP2350 via SPI.
- **18x SM16206S** — RGB LED driver/shift-register chips, 9 per
  wing. **Parallel-GPIO topology**: each chip has its own dedicated data
  input line (not daisy-chained). CLK is shared per wing; LE and OE are
  each a single line shared across **both** wings.
  - 16 output channels per chip (48 LEDs/wing × 3ch ÷ 9 chips/wing).
  - Rated SPI/shift clock: 25MHz continuous, 30MHz absolute max.
- **2x A3144E** hall-effect sensors (HE_0, HE_1), one at the end of each wing.

- Wireless power receiver — rated 5V/2A; real deliverable current is
  considerably below that rating

## RP2350 pin map

| GPIO | Function |
|---|---|
| 0,1,2,3,4,6,7,8,10 | LHS driver data lines, chip 0 (centre) → chip 8 (edge) |
| 5 | HE_1 |
| 9 | OE (shared, both wings) |
| 11 | LHS CLK |
| 12 | RP_SPI MOSI (hardware SPI1 peripheral pin) |
| 13 | RP_SPI CSn |
| 14 | RP_SPI SCK |
| 15 | RP_SPI MISO |
| 16 | RP_ESP32_GPIO — spare line to ESP32, purpose unassigned |
| 18 | RHS CLK |
| 19 | LE (shared, both wings) |
| 20 | HE_0 |
| 21–29 | RHS driver data lines, chip 0 (centre) → chip 8 (edge) |

LHS data pins, index = chip 0..8: `10, 8, 7, 6, 4, 3, 2, 1, 0`
RHS data pins, index = chip 0..8: `21, 22, 23, 24, 25, 26, 27, 28, 29`


GPIO12–15 are the RP2350's hardware SPI1 peripheral pins (fixed
RX/TX/SCK/CSn assignment, not relocatable). The RP2350 is configured as
**SPI slave**; the ESP32 is **SPI master**.


GPIO16 (RP_ESP32_GPIO) is a wired but functionally unassigned spare
  line between the two MCUs.

## ESP32-C3-MINI-1-H4 pin map

| GPIO | Function
|---|---|
| 0 | RP_ESP_GPIO (Spare) |
| 2 | MISO (FSPIQ, dedicated IO_MUX pin) |
| 6 | SCK (FSPICLK, IO_MUX) |
| 7 | MOSI (FSPID, IO_MUX) |
| 10 | CS (FSPICS0, IO_MUX) |

All 4 SPI signals sit on the ESP32's dedicated IO_MUX path - supports 80 MHz.
## LED-to-chip wiring (per wing; LHS and RHS are identical patterns)


Each chip has 16 output channels, numbered 0–15 in shift-register order
(stage 0 = first bit shifted in). 9 chips per wing, numbered 0 (centre)
→ 8 (edge) per the GPIO pin map above.



**LED → (chip, output pin) table**, LED 0–47 for each wing, R/G/B channels
| LED | R (chip,pin) | G (chip,pin) | B (chip,pin) |
|---|---|---|---|
| 0 | (0,0) | (0,14) | (0,15) |
| 1 | (0,1) | (0,4) | (0,2) |
| 2 | (0,3) | (0,13) | (0,6) |
| 3 | (0,5) | (0,11) | (0,12) |
| 4 | (0,7) | (0,9) | (0,10) |
| 5 | (1,0) | (1,15) | (0,8) |
| 6 | (1,1) | (1,3) | (1,14) |
| 7 | (1,2) | (1,13) | (1,5) |
| 8 | (1,4) | (1,11) | (1,12) |
| 9 | (1,6) | (1,9) | (1,10) |
| 10 | (1,7) | (2,15) | (1,8) |
| 11 | (2,0) | (2,1) | (2,14) |
| 12 | (2,2) | (2,6) | (2,4) |
| 13 | (2,3) | (2,12) | (2,13) |
| 14 | (2,5) | (2,10) | (2,11) |
| 15 | (2,7) | (2,8) | (2,9) |
| 16 | (3,0) | (3,14) | (3,15) |
| 17 | (3,1) | (3,4) | (3,2) |
| 18 | (3,3) | (3,13) | (3,6) |
| 19 | (3,5) | (3,11) | (3,12) |
| 20 | (3,7) | (3,9) | (3,10) |
| 21 | (4,0) | (4,15) | (3,8) |
| 22 | (4,1) | (4,3) | (4,14) |
| 23 | (4,2) | (4,13) | (4,5) |
| 24 | (4,4) | (4,11) | (4,12) |
| 25 | (4,6) | (4,9) | (4,10) |
| 26 | (4,7) | (5,15) | (4,8) |
| 27 | (5,0) | (5,1) | (5,14) |
| 28 | (5,2) | (5,6) | (5,4) |
| 29 | (5,3) | (5,12) | (5,13) |
| 30 | (5,5) | (5,10) | (5,11) |
| 31 | (5,7) | (5,8) | (5,9) |
| 32 | (6,0) | (6,14) | (6,15) |
| 33 | (6,1) | (6,4) | (6,2) |
| 34 | (6,3) | (6,13) | (6,6) |
| 35 | (6,5) | (6,11) | (6,12) |
| 36 | (6,7) | (6,9) | (6,10) |
| 37 | (7,0) | (7,15) | (6,8) |
| 38 | (7,1) | (7,3) | (7,14) |
| 39 | (7,2) | (7,13) | (7,5) |
| 40 | (7,4) | (7,11) | (7,12) |
| 41 | (7,6) | (7,9) | (7,10) |
| 42 | (7,7) | (8,15) | (7,8) |
| 43 | (8,0) | (8,1) | (8,14) |
| 44 | (8,2) | (8,6) | (8,4) |
| 45 | (8,3) | (8,12) | (8,13) |
| 46 | (8,5) | (8,10) | (8,11) |
| 47 | (8,7) | (8,8) | (8,9) |

