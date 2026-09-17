#pragma once

// Writes the built-in test animation to flash
void animation_init(void);

// Core 1 entry point: plays the stored animation, building the next frame into
// the frame buffer's back buffer while render.c displays the current one
void animation_task(void);
