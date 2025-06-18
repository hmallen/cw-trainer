#include "web_server.h"
#include "trainer_status.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "driver/uart.h"
#include <string.h>

static httpd_handle_t server = NULL;
static char g_last_cmd[64] = ""; // stores most recent control command

static cJSON *status_to_json(void)
{
    const trainer_status_t *s = &g_status;
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "lesson", s->lesson);
    cJSON_AddNumberToObject(root, "frequency", s->frequency);
    cJSON_AddNumberToObject(root, "speed", s->speed);
    cJSON_AddNumberToObject(root, "effectiveSpeed", s->effective_speed);
    cJSON_AddNumberToObject(root, "accuracy", s->accuracy);
    cJSON_AddBoolToObject(root, "decoderEnabled", s->decoder_enabled);
    cJSON_AddBoolToObject(root, "kochMode", s->koch_mode);
    cJSON_AddStringToObject(root, "currentText", s->current_text);
    cJSON_AddStringToObject(root, "decodedText", s->decoded_text);
    cJSON_AddNumberToObject(root, "sessions", s->sessions);
    cJSON_AddNumberToObject(root, "characters", s->characters);
    cJSON_AddNumberToObject(root, "bestWPM", s->best_wpm);
    cJSON_AddStringToObject(root, "waveform", s->waveform);
    cJSON_AddStringToObject(root, "output", s->output);
    cJSON_AddBoolToObject(root, "sending", s->sending);
    cJSON_AddBoolToObject(root, "listening", s->listening);
    return root;
}

// --- /api/control POST handler -------------------------------------------------
static esp_err_t api_control_post(httpd_req_t *req)
{
    int total = req->content_len;
    if (total <= 0 || total > 256) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty or too large body");
        return ESP_FAIL;
    }

    char buf[257];
    int received = httpd_req_recv(req, buf, total);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
        return ESP_FAIL;
    }
    buf[received] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
    if (!cJSON_IsString(cmd)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'cmd' string");
        return ESP_FAIL;
    }

    const char *cmd_str = cmd->valuestring;
    uart_write_bytes(UART_NUM_1, cmd_str, strlen(cmd_str));
    strncpy(g_last_cmd, cmd_str, sizeof(g_last_cmd) - 1);
    uart_write_bytes(UART_NUM_1, "\n", 1);

    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

// --- /api/control GET handler --------------------------------------------------
static esp_err_t api_control_get(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "lastCmd", g_last_cmd);
    char *out = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, out);
    cJSON_Delete(root);
    free(out);
    return ESP_OK;
}

// --- /api/stats GET/POST handlers ---------------------------------------------
static cJSON *stats_to_json(void)
{
    const trainer_status_t *s = &g_status;
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "sessions", s->sessions);
    cJSON_AddNumberToObject(root, "characters", s->characters);
    cJSON_AddNumberToObject(root, "bestWPM", s->best_wpm);
    return root;
}

static esp_err_t api_stats_get(httpd_req_t *req)
{
    cJSON *json = stats_to_json();
    char *out = cJSON_PrintUnformatted(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, out);
    cJSON_Delete(json);
    free(out);
    return ESP_OK;
}

static esp_err_t api_stats_post(httpd_req_t *req)
{
    int len = req->content_len;
    char buf[64];
    if (len <= 0 || len >= (int)sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Body too large or empty");
        return ESP_FAIL;
    }
    int r = httpd_req_recv(req, buf, len);
    if (r <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read body");
        return ESP_FAIL;
    }
    buf[r] = '\0';
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    cJSON *reset = cJSON_GetObjectItem(root, "reset");
    if (cJSON_IsBool(reset) && cJSON_IsTrue(reset)) {
        trainer_status_reset();
    }
    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

// Existing status GET handler
static esp_err_t api_status_get(httpd_req_t *req)
{
    cJSON *json = status_to_json();
    char *buf = cJSON_PrintUnformatted(json);
    httpd_resp_set_type(req, "application/json");
    esp_err_t r = httpd_resp_sendstr(req, buf);
    cJSON_Delete(json);
    free(buf);
    return r;
}

static esp_err_t static_get(httpd_req_t *req)
{
    const char *filepath = strcmp(req->uri, "/") == 0 ? "index.html"
                                                      : req->uri + 1; // skip leading '/'
    char full[80];
    snprintf(full, sizeof(full), "/spiffs/%s", filepath);

    FILE *f = fopen(full, "r");
    if (!f) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    // simple MIME detection
    const char *ext = strrchr(full, '.');
    if (ext) {
        if (strcmp(ext, ".css") == 0) {
            httpd_resp_set_type(req, "text/css");
        } else if (strcmp(ext, ".js") == 0) {
            httpd_resp_set_type(req, "application/javascript");
        } else if (strcmp(ext, ".html") == 0) {
            httpd_resp_set_type(req, "text/html");
        } else if (strcmp(ext, ".json") == 0) {
            httpd_resp_set_type(req, "application/json");
        } else {
            httpd_resp_set_type(req, "text/plain");
        }
    } else {
        httpd_resp_set_type(req, "text/plain");
    }
    char buf[256];
    while (size) {
        int to_read = size > sizeof(buf) ? sizeof(buf) : size;
        fread(buf, 1, to_read, f);
        httpd_resp_send_chunk(req, buf, to_read);
        size -= to_read;
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0); // end
    return ESP_OK;
}

void start_webserver(void)
{
    if (server) return; // already running

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard; // allow wildcard if needed later

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t status_uri = {
            .uri       = "/api/status",
            .method    = HTTP_GET,
            .handler   = api_status_get,
            .user_ctx  = NULL};
        httpd_register_uri_handler(server, &status_uri);

        // Register /api/control GET
    httpd_uri_t control_get = {
        .uri = "/api/control",
        .method = HTTP_GET,
        .handler = api_control_get,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &control_get);

    // Register /api/control POST
        httpd_uri_t control_post = {
            .uri = "/api/control",
            .method = HTTP_POST,
            .handler = api_control_post,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &control_post);

    // Register /api/stats GET
    httpd_uri_t stats_get = {
        .uri = "/api/stats",
        .method = HTTP_GET,
        .handler = api_stats_get,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &stats_get);

    // Register /api/stats POST
    httpd_uri_t stats_post = {
        .uri = "/api/stats",
        .method = HTTP_POST,
        .handler = api_stats_post,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &stats_post);
    }

    // Register static file handler
    httpd_uri_t static_handler = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = static_get,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &static_handler);
}
