#pragma once

// Starts the board's own WiFi network (SoftAP) - connect phone/PC directly to this
// The board is reachable at 192.168.4.1 or <hostname>.local once connected
void wifi_ap_init(void);
