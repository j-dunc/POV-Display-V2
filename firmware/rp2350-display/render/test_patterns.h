#pragma once

#include "frame_geometry.h"

// Test pattern - one channel (RED/GREEN/BLUE) ramps 0 to full scale
void test_pattern_build_channel_ramp(int channel);

// Test pattern - all channels ramp together 0 to full scale
void test_pattern_build_gray_ramp(void);

// Test pattern - flat full-scale white everywhere
void test_pattern_build_radial_white(void);

// Test pattern - a smooth rainbow, one full turn of hue around the circle
void test_pattern_build_rainbow(void);
