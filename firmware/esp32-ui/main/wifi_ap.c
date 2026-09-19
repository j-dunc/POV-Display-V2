// wifi_ap: starts the board's own SoftAP WiFi network, so a phone/PC can
// connect directly without an existing network

#include "wifi_ap.h"

#include <string.h>

#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mdns.h"

static const char *TAG = "wifi_ap";

// Set to 1 to join existing WiFi in station mode
// Set to 0 to host WiFi AP
#define WIFI_CONNECT_TO_EXISTING_AP 0
#define WIFI_STA_SSID CONFIG_ESP_STA_WIFI_SSID
#define WIFI_STA_PASSWORD CONFIG_ESP_STA_WIFI_PASSWORD

#if WIFI_CONNECT_TO_EXISTING_AP

// Callback invoked on wifi/ip events - log connecting/disconnecting and the assigned IP
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "disconnected from %s, retrying", WIFI_STA_SSID);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "connected to %s, got IP:" IPSTR, WIFI_STA_SSID, IP2STR(&event->ip_info.ip));
    }
}

#else

// Callback invoked on wifi events - log joining/leaving
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " joined", MAC2STR(event->mac));
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " left", MAC2STR(event->mac));
    }
}

#endif

#if WIFI_CONNECT_TO_EXISTING_AP

void wifi_ap_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_STA_SSID,
            .password = WIFI_STA_PASSWORD,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    ESP_LOGI(TAG, "connecting to %s...", WIFI_STA_SSID);

    // Setup mdns to resolve <hostname>.local
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(CONFIG_MDNS_HOSTNAME));
    ESP_ERROR_CHECK(mdns_instance_name_set("POV Display"));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0)); // Advertise HTTP on port 80
    ESP_LOGI(TAG, "reachable at http://%s.local", CONFIG_MDNS_HOSTNAME);
}

#else

void wifi_ap_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    // Access point config
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .ssid_len = strlen(CONFIG_ESP_WIFI_SSID),
            .channel = CONFIG_ESP_WIFI_CHANNEL,
            .password = CONFIG_ESP_WIFI_PASSWORD,
            .max_connection = CONFIG_ESP_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = true,
            },
        },
    };
    // WPA2 requires a non-empty password - fall back to an open network
    if (strlen(CONFIG_ESP_WIFI_PASSWORD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start()); // network starts broadcasting here

    ESP_LOGI(TAG, "started, SSID:%s password:%s channel:%d",
             CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD, CONFIG_ESP_WIFI_CHANNEL);

    // Setup mdns to resolve <hostname>.local
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(CONFIG_MDNS_HOSTNAME));
    ESP_ERROR_CHECK(mdns_instance_name_set("POV Display"));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0)); // Advertise HTTP on port 80
    ESP_LOGI(TAG, "reachable at http://%s.local", CONFIG_MDNS_HOSTNAME);
}

#endif
