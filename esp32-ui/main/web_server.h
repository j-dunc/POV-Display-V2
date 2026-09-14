#pragma once

// Starts the HTTP server and registers its handlers:
// GET "/" - serves the image upload/converter page (converter.html)
// POST "/frame" - pushes one frame's worth of raw 0-255 RGB bytes to rp2350 via SPI
// Call after AP init
void web_server_start(void);
