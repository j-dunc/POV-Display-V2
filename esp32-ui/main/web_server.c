#include "web_server.h"

#include "esp_http_server.h"
#include "esp_log.h"

#include "spi_link.h"

static const char *TAG = "web_server";

// Placeholder page
static const char index_html[] =
    "<!DOCTYPE html><html><head><title>POV Display</title></head>"
    "<body><h1>POV Display</h1></body></html>";

// Handles request defined by index_uri below
static esp_err_t index_get_handler(httpd_req_t *req) {
    httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Routing table entry: GET "/" -> index_get_handler
static const httpd_uri_t index_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_get_handler,
};

#define FRAME_BYTES (NUM_COLUMNS * LEDS_PER_WING * CHANNELS)

static uint8_t frame_buf[NUM_COLUMNS][LEDS_PER_WING][CHANNELS];

// Setup headers for POST and OPTIONS
static void add_cors_headers(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
}

// OPTIONS needed if POST sent from external origin
static esp_err_t frame_options_handler(httpd_req_t *req) {
    add_cors_headers(req);
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// Accepts one frame of 0-255 RGB and sends to RP2350
static esp_err_t frame_post_handler(httpd_req_t *req) {
    add_cors_headers(req);

    // Check frame size
    if (req->content_len != FRAME_BYTES) {
        ESP_LOGW(TAG, "wrong frame size: got %d, expected %d", (int)req->content_len, FRAME_BYTES);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "wrong frame size");
        return ESP_FAIL;
    }

    uint8_t *dst = (uint8_t *)frame_buf; // destination is frame buffer
    int received = 0;
    // Repeat request until all bytes received
    while (received < FRAME_BYTES) {
        int ret = httpd_req_recv(req, (char *)(dst + received), FRAME_BYTES - received); // returns num of bytes received, dst+received ensures data written into next free part of buffer
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue; // retry on timeout
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "receive failed");
            return ESP_FAIL;
        }
        received += ret;
    }

    ESP_LOGI(TAG, "received frame, pushing to RP2350");
    if (!spi_link_push_frame(frame_buf)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "push to RP2350 failed");
        return ESP_FAIL;
    }

    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Routing table entries for POST and OPTIONS for /frame 
static const httpd_uri_t frame_post_uri = {
    .uri = "/frame",
    .method = HTTP_POST,
    .handler = frame_post_handler,
};

static const httpd_uri_t frame_options_uri = {
    .uri = "/frame",
    .method = HTTP_OPTIONS,
    .handler = frame_options_handler,
};

void web_server_start(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    ESP_LOGI(TAG, "starting server on port %d", config.server_port);
    ESP_ERROR_CHECK(httpd_start(&server, &config));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &index_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &frame_post_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &frame_options_uri));
}
