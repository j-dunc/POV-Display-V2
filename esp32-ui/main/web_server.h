#pragma once

// Starts the HTTP server and registers its handlers:
// GET "/" - redirects to "/upload"
// GET "/upload" - serves the image upload/converter page (converter.html)
// GET "/calibrate" - serves the colour calibration tuning page (calibrate.html)
// GET "/status.js" - serves the shared live-status bar script, used by both pages above
// POST "/frame" - pushes one frame's worth of raw 0-255 RGB bytes to rp2350 via SPI
// GET "/calibration" - reads the rp2350's current live colour calibration via SPI
// POST "/calibration" - pushes new colour calibration (r/g/b scale + gamma) to rp2350 via SPI
// GET "/status" - reports the rp2350's current status (rpm, uptime) as JSON, from an SPI query
// POST "/test-pattern" - displays one of the rp2350's built-in calibration test patterns
// Call after AP init
void web_server_start(void);
