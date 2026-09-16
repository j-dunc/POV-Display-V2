#pragma once

// Starts the HTTP server and registers its handlers:
// GET "/" - serves the image upload/converter page (converter.html)
// GET "/calibrate" - serves the colour calibration tuning page (calibrate.html)
// POST "/frame" - pushes one frame's worth of raw 0-255 RGB bytes to rp2350 via SPI
// GET "/calibration" - reads the rp2350's current live colour calibration via SPI
// POST "/calibration" - pushes new colour calibration (r/g/b scale + gamma) to rp2350 via SPI
// Call after AP init
void web_server_start(void);
