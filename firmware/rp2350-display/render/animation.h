#pragma once

// Builds the next frame of the stored animation into the frame buffer's back buffer,
// if it's free. Returns straight away when there's nothing to do, so core 1 can call
// it in the gaps between servicing SPI
void animation_build_next_frame(void);

// Restarts playback from the first frame
void animation_reset(void);
