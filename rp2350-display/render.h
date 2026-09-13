#pragma once

// Blank below this RPM - also the spinning threshold reported over CMD_GET_STATUS
#define MIN_READABLE_RPM 100.0f

/*
Render loop: shows whichever precomputed image column matches the current
shaft angle. Ties together hall_sensor (angle/rpm), image_source
(precomputed columns) and led_shift (hardware shift-out)
*/

// Builds image
void render_init(void);


/*
Call continuously from the main loop. Shifts out a new column whenever the
shaft has rotated into the next one; blanks the display (OE high) if the
angle isn't known yet or the board isn't spinning fast enough for
persistence of vision to blend columns into a continuous image.
*/
void render_update(void);
