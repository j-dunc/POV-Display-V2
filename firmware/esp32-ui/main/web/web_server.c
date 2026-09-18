// web_server: HTTP server serving the upload/calibration pages and their
// APIs, relaying frame/calibration/status requests to the RP2350 over SPI

#include "web_server.h"

#include "esp_http_server.h"
#include "esp_log.h"

#include "spi_link.h"

#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "web_server";

// converter.html, calibrate.html and status.js from embedded text files
extern const uint8_t converter_html_start[] asm("_binary_converter_html_start");
extern const uint8_t converter_html_end[] asm("_binary_converter_html_end");
extern const uint8_t calibrate_html_start[] asm("_binary_calibrate_html_start");
extern const uint8_t calibrate_html_end[] asm("_binary_calibrate_html_end");
extern const uint8_t status_js_start[] asm("_binary_status_js_start");
extern const uint8_t status_js_end[] asm("_binary_status_js_end");

// Handles request defined by upload_uri below
static esp_err_t upload_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)converter_html_start,
                     converter_html_end - converter_html_start - 1); // -1: remove null terminator at end
    return ESP_OK;
}

// Routing table entry: GET "/upload" -> upload_get_handler
static const httpd_uri_t upload_uri = {
    .uri = "/upload",
    .method = HTTP_GET,
    .handler = upload_get_handler,
};

// Redirects bare "/" (e.g. pov.local) to "/upload"
static esp_err_t index_get_handler(httpd_req_t *req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/upload");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// Routing table entry: GET "/" -> index_get_handler
static const httpd_uri_t index_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_get_handler,
};

// Handles request defined by calibrate_uri below
static esp_err_t calibrate_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)calibrate_html_start,
                     calibrate_html_end - calibrate_html_start - 1); // -1: remove null terminator at end
    return ESP_OK;
}

// Routing table entry: GET "/calibrate" -> calibrate_get_handler
static const httpd_uri_t calibrate_uri = {
    .uri = "/calibrate",
    .method = HTTP_GET,
    .handler = calibrate_get_handler,
};

// Handles request defined by status_js_uri below
static esp_err_t status_js_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char *)status_js_start,
                     status_js_end - status_js_start - 1); // -1: remove null terminator at end
    return ESP_OK;
}

// Routing table entry: GET "/status.js" -> status_js_get_handler
static const httpd_uri_t status_js_uri = {
    .uri = "/status.js",
    .method = HTTP_GET,
    .handler = status_js_get_handler,
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

// Receives exactly len bytes, looping since one recv can return a partial read
static bool recv_exact(httpd_req_t *req, uint8_t *buf, int len) {
    int received = 0;
    while (received < len) {
        int ret = httpd_req_recv(req, (char *)(buf + received), len - received);
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue; // retry on timeout
        if (ret <= 0) return false;
        received += ret;
    }
    return true;
}

// Accepts whole animation as raw 0-255 RGB, frames in order, each with columns in order
// Frame count is from body length
// Streamed straight to RP2350 a column at a time rather than holding in RAM
// Optional ?revs=N sets how many revolutions each frame is held for, setting playback speed
static esp_err_t animation_post_handler(httpd_req_t *req) {
    add_cors_headers(req);

    // Validate content length
    if (req->content_len <= 0 || req->content_len % FRAME_BYTES != 0) {
        ESP_LOGW(TAG, "animation size %d is not a whole number of %d-byte frames", (int)req->content_len, FRAME_BYTES);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "size must be a whole number of frames");
        return ESP_FAIL;
    }
    int frame_count = req->content_len / FRAME_BYTES;
    if (frame_count > MAX_ANIMATION_FRAMES) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "too many frames for the flash region");
        return ESP_FAIL;
    }

    // Playback speed, defaulting to 2 revolutions per frame (~10fps at 1200rpm)
    int revs_per_frame = 2;
    char query[32];
    char value[8];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK
        && httpd_query_key_value(query, "revs", value, sizeof(value)) == ESP_OK) {
        revs_per_frame = atoi(value); // to int
    }
    if (revs_per_frame < 1) revs_per_frame = 1;

    ESP_LOGI(TAG, "animation upload: %d frames, %d revs/frame", frame_count, revs_per_frame);
    if (!spi_link_animation_begin(frame_count, revs_per_frame)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "RP2350 did not accept the upload");
        return ESP_FAIL;
    }

    // Send one column across SPI at a time - only receive one column from http at a time to save RAM
    uint8_t column[LEDS_PER_WING][CHANNELS];
    for (int frame = 0; frame < frame_count; frame++) {
        for (int col = 0; col < NUM_COLUMNS; col++) {
            if (!recv_exact(req, (uint8_t *)column, sizeof(column))) { // only receive one column
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "receive failed");
                return ESP_FAIL;
            }
            if (!spi_link_animation_column(col, column)) { // send the one column
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "push to RP2350 failed");
                return ESP_FAIL;
            }
        }
    }

    if (!spi_link_animation_end()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "RP2350 did not finish the upload");
        return ESP_FAIL;
    }

    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const httpd_uri_t animation_post_uri = {
    .uri = "/animation",
    .method = HTTP_POST,
    .handler = animation_post_handler,
};

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

// OPTIONS needed if POST sent from external origin
static esp_err_t calibration_options_handler(httpd_req_t *req) {
    add_cors_headers(req);
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// Accepts 8 little-endian float32s (r_scale, g_scale, b_scale, gamma, radial_gamma, max_duty_fraction, dither_enabled, bcm_bits) and forwards to RP2350
static esp_err_t calibration_post_handler(httpd_req_t *req) {
    add_cors_headers(req);

    if (req->content_len != CALIBRATION_PAYLOAD_SIZE) {
        ESP_LOGW(TAG, "wrong calibration size: got %d, expected %d", (int)req->content_len, (int)CALIBRATION_PAYLOAD_SIZE);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "wrong calibration size");
        return ESP_FAIL;
    }

    // Repeat request until all bytes recieved
    calibration_payload_t payload;
    uint8_t *dst = (uint8_t *)&payload;
    int received = 0;
    while (received < (int)CALIBRATION_PAYLOAD_SIZE) {
        int ret = httpd_req_recv(req, (char *)(dst + received), CALIBRATION_PAYLOAD_SIZE - received);
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue; // retry on timeout
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "receive failed");
            return ESP_FAIL;
        }
        received += ret;
    }

    // Send calibration values
    if (!spi_link_set_calibration(payload.r_scale, payload.g_scale, payload.b_scale, payload.gamma, payload.radial_gamma, payload.max_duty_fraction, payload.dither_enabled != 0.0f, payload.bcm_bits)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "push to RP2350 failed");
        return ESP_FAIL;
    }

    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Reports the RP2350's current live calibration values, so the UI can show real values
static esp_err_t calibration_get_handler(httpd_req_t *req) {
    add_cors_headers(req);

    calibration_payload_t payload;
    if (!spi_link_get_calibration(&payload)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "read from RP2350 failed");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/octet-stream");
    httpd_resp_send(req, (const char *)&payload, sizeof(payload));
    return ESP_OK;
}

// Routing table entries for GET, POST and OPTIONS for /calibration
static const httpd_uri_t calibration_get_uri = {
    .uri = "/calibration",
    .method = HTTP_GET,
    .handler = calibration_get_handler,
};

static const httpd_uri_t calibration_post_uri = {
    .uri = "/calibration",
    .method = HTTP_POST,
    .handler = calibration_post_handler,
};

static const httpd_uri_t calibration_options_uri = {
    .uri = "/calibration",
    .method = HTTP_OPTIONS,
    .handler = calibration_options_handler,
};

// Reports the RP2350's current status as JSON, from an SPI transaction
static esp_err_t status_get_handler(httpd_req_t *req) {
    add_cors_headers(req);

    spi_status_t status;
    bool linked = spi_link_get_status(&status);

    // Format JSON response
    char resp[128];
    int len = linked
        ? snprintf(resp, sizeof(resp),
                   "{\"linked\":true,\"rpm\":%.1f,\"rp2350_uptime_ms\":%lu}", // in JSON
                   status.rpm, (unsigned long)status.uptime_ms)
        : snprintf(resp, sizeof(resp), "{\"linked\":false}");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, len);
    return ESP_OK;
}

static const httpd_uri_t status_get_uri = {
    .uri = "/status",
    .method = HTTP_GET,
    .handler = status_get_handler,
};

// OPTIONS needed if POST sent from external origin
static esp_err_t test_pattern_options_handler(httpd_req_t *req) {
    add_cors_headers(req);
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// Accepts a 1-byte TEST_PATTERN_* id and forwards to RP2350
static esp_err_t test_pattern_post_handler(httpd_req_t *req) {
    add_cors_headers(req);

    if (req->content_len != TEST_PATTERN_PAYLOAD_SIZE) {
        ESP_LOGW(TAG, "wrong test pattern size: got %d, expected %d", (int)req->content_len, TEST_PATTERN_PAYLOAD_SIZE);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "wrong test pattern size");
        return ESP_FAIL;
    }

    uint8_t pattern_id;
    int ret = httpd_req_recv(req, (char *)&pattern_id, TEST_PATTERN_PAYLOAD_SIZE);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "receive failed");
        return ESP_FAIL;
    }

    if (!spi_link_show_test_pattern(pattern_id)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "push to RP2350 failed");
        return ESP_FAIL;
    }

    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const httpd_uri_t test_pattern_post_uri = {
    .uri = "/test-pattern",
    .method = HTTP_POST,
    .handler = test_pattern_post_handler,
};

static const httpd_uri_t test_pattern_options_uri = {
    .uri = "/test-pattern",
    .method = HTTP_OPTIONS,
    .handler = test_pattern_options_handler,
};

void web_server_start(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16; // default of 8 isn't enough for all the routes registered below

    ESP_LOGI(TAG, "starting server on port %d", config.server_port);
    ESP_ERROR_CHECK(httpd_start(&server, &config));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &index_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &upload_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &calibrate_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &status_js_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &animation_post_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &frame_post_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &frame_options_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &calibration_get_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &calibration_post_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &calibration_options_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &status_get_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &test_pattern_post_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &test_pattern_options_uri));
}
