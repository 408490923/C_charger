/*
 * WebSocket (TCP) server for the web client.
 *
 * Runs alongside the legacy UDP server (port 8000, used by `make ota`).
 * The browser connects directly to ws://<device-ip>:81/ws, sends control
 * commands (same "key=\"value\"" format as before) and receives live status
 * frames pushed every 300 ms. No external bridge process is required.
 */
#include "ws_server.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "http_client.h"
#include "ota.h"
#include "udp_server.h"
#include "menu.h"

#define WS_PORT 81
static const char *TAG = "ws_server";

static httpd_handle_t g_server = NULL;
static int g_fd = -1;               /* socket fd of the active client */

/* Globals owned by other translation units. */
extern char city[50];
extern int16_t rgbProportion[3];
extern int16_t light;

/* Minimal URL-encoder (same rule as the old UDP path). */
static void url_encode(const char *str, int strSize, char *result, int resultSize)
{
    int i, j = 0;
    char ch;
    if (!str || !result || strSize <= 0 || resultSize <= 0) return;
    for (i = 0; (i < strSize) && (j < resultSize); i++) {
        ch = str[i];
        if ((ch >= 'A') && (ch <= 'Z')) result[j++] = ch;
        else if ((ch >= 'a') && (ch <= 'z')) result[j++] = ch;
        else if ((ch >= '0') && (ch <= '9')) result[j++] = ch;
        else if (ch == ' ') result[j++] = '+';
        else if (j + 3 < resultSize) { sprintf(result + j, "%%%02X", (unsigned char)ch); j += 3; }
        else return;
    }
    if (j >= 0) result[j] = '\0';
}

/* Parse one browser command (text frame) and apply it. */
static void ws_apply_cmd(const char *txt)
{
    char tmp[256];
    if (cutString("city", tmp, (char *)txt) == 0) {
        url_encode(tmp, strlen(tmp), city, sizeof(city));
    }
    if (cutString("ota_url", tmp, (char *)txt) == 0) {
        trigger_ota_url(tmp);
    }
    if (cutString("rgb[0]", tmp, (char *)txt) == 0) {
        rgbProportion[0] = (tmp[0] - '0') * 100 + (tmp[1] - '0') * 10 + (tmp[2] - '0');
    }
    if (cutString("rgb[1]", tmp, (char *)txt) == 0) {
        rgbProportion[1] = (tmp[0] - '0') * 100 + (tmp[1] - '0') * 10 + (tmp[2] - '0');
    }
    if (cutString("rgb[2]", tmp, (char *)txt) == 0) {
        rgbProportion[2] = (tmp[0] - '0') * 100 + (tmp[1] - '0') * 10 + (tmp[2] - '0');
    }
    if (cutString("light", tmp, (char *)txt) == 0) {
        light = (tmp[0] - '0') * 100 + (tmp[1] - '0') * 10 + (tmp[2] - '0');
    }
    if (cutString("oledbright", tmp, (char *)txt) == 0) {
        int b = atoi(tmp);
        if (b < 0) b = 0;
        if (b > 255) b = 255;
        oledSetBrightness((uint8_t)b);
    }
    nvsWrite();
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    /* Handshake: httpd calls the handler once with method GET when the
     * WebSocket connection is opened. Record the fd and return immediately
     * (never block here, or the single httpd worker thread gets stuck). */
    if (req->method == HTTP_GET) {
        g_fd = httpd_req_to_sockfd(req);
        ESP_LOGI(TAG, "client connected, fd=%d", g_fd);
        return ESP_OK;
    }

    /* Data frame: handle exactly one frame per invocation (non-blocking). */
    httpd_ws_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &frame, 0);   /* read frame length */
    if (ret != ESP_OK) return ret;

    if (frame.len && frame.len < 1024) {
        uint8_t *buf = calloc(1, frame.len + 1);
        if (!buf) return ESP_ERR_NO_MEM;
        frame.payload = buf;
        ret = httpd_ws_recv_frame(req, &frame, frame.len);
        if (ret == ESP_OK && frame.type == HTTPD_WS_TYPE_TEXT) {
            ws_apply_cmd((char *)buf);
            char out[256];
            int n = charger_build_status(out, sizeof(out));
            httpd_ws_frame_t outframe = {
                .type = HTTPD_WS_TYPE_TEXT,
                .payload = (uint8_t *)out,
                .len = (uint32_t)n
            };
            httpd_ws_send_frame(req, &outframe);
        }
        free(buf);
    }

    g_fd = httpd_req_to_sockfd(req);
    return ESP_OK;
}

/* Push live status to the connected browser every 200 ms. */
static void ws_push_task(void *arg)
{
    char buf[256];
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(200));
        if (g_fd >= 0 && g_server) {
            int n = charger_build_status(buf, sizeof(buf));
            httpd_ws_frame_t f = {
                .type = HTTPD_WS_TYPE_TEXT,
                .payload = (uint8_t *)buf,
                .len = (uint32_t)n
            };
            if (httpd_ws_send_frame_async(g_server, g_fd, &f) != ESP_OK) {
                g_fd = -1;   /* client gone; stop pushing until it reconnects */
            }
        }
    }
}

void ws_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = WS_PORT;
    config.max_uri_handlers = 2;
    config.max_open_sockets = 3;         /* keep LWIP socket usage low (LWIP_MAX_SOCKETS=10) */
    config.stack_size = 8192;
    config.task_priority = tskIDLE_PRIORITY + 5;

    httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_handler,
        .is_websocket = true,
        .handle_ws_control_frames = true,
    };

    if (httpd_start(&g_server, &config) == ESP_OK) {
        httpd_register_uri_handler(g_server, &ws_uri);
        xTaskCreate(ws_push_task, "ws_push", 4096, NULL, 4, NULL);
        ESP_LOGI(TAG, "WebSocket server started on :%d/ws", WS_PORT);
    } else {
        ESP_LOGE(TAG, "Failed to start WebSocket server");
    }
}
