#pragma once

// Blank below this RPM
#define MIN_READABLE_RPM 100.0f

// Default number of revolutions each animation frame is held for
#define DEFAULT_REVS_PER_FRAME 2

/*
Render loop: shows whichever precomputed image column matches the current
shaft angle. Ties together hall_sensor (angle/rpm), image_source
(precomputed columns) and led_shift (hardware shift-out)
*/

// Builds image
void render_init(void);

/*
Sets how many revolutions each animation frame is held for before the next one
swaps in - lower is faster playback. Frames only ever swap on a revolution
boundary, so playback fps is (revolutions per second / revs_per_frame)
*/
void render_set_revs_per_frame(int revs_per_frame);

/*
Call continuously from the main loop. Shifts out a new column whenever the
shaft has rotated into the next one; blanks the display (OE high) if the
angle isn't known yet or the board isn't spinning fast enough for
persistence of vision to blend columns into a continuous image.
*/
void render_update(void);
