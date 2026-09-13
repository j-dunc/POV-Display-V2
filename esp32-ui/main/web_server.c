#include "web_server.h"

#include "esp_http_server.h"
#include "esp_log.h"

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

void web_server_start(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    ESP_LOGI(TAG, "starting server on port %d", config.server_port);
    ESP_ERROR_CHECK(httpd_start(&server, &config));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &index_uri));
}
