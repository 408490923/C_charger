/* ESP HTTP Client Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "menu.h"

#include "esp_http_client.h"
#include "http_client.h"
extern char city[20];

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048
static const char *TAG = "HTTP_CLIENT";
char local_response_buffer[2048] = {1};


/* Root cert for howsmyssl.com, taken from howsmyssl_com_root_cert.pem

   The PEM file was extracted from the output of this command:
   openssl s_client -showcerts -connect www.howsmyssl.com:443 </dev/null

   The CA root cert is the last cert given in the chain of certs.

   To embed it in the app binary, the PEM file is named
   in the component.mk COMPONENT_EMBED_TXTFILES variable.
*/

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer;  // Buffer to store response of http request from event handler
    static int output_len;       // Stores number of bytes read
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // If user_data buffer is configured, copy the response into the buffer
                if (evt->user_data) {
                    memcpy(evt->user_data + output_len, evt->data, evt->data_len);
                } else {
                    if (output_buffer == NULL) {
                        output_buffer = (char *) malloc(esp_http_client_get_content_length(evt->client));
                        output_len = 0;
                        if (output_buffer == NULL) {
                            ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                            return ESP_FAIL;
                        }
                    }
                    memcpy(output_buffer + output_len, evt->data, evt->data_len);
                }
                output_len += evt->data_len;
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (output_buffer != NULL) {
                // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
                // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
    }
    return ESP_OK;
}

int findMenu(const char * menu, char *buffer)
{
    int mLen = strlen(menu);
    int bLen = strlen(buffer);
    
    int i = 0;
    int j = 0;
    for(i = 0; i < bLen; i++)
    {
        for(j = 0; j < mLen && (i + j < bLen); j++)
        {
            if(menu[j] == buffer[i + j])
            {

            }
            else
                break;
            if(j == mLen - 1)
                return i + j + 2;
        }
    }
    return -1;

}

int cutString(const char * menu, char *target, char *buffer)
{
    
    int position = findMenu(menu, buffer);
    if(position == -1)
        return -1;
    ESP_LOGI(TAG, "position %d", position);
    //int state = 0;
    int i = 0;
    int j = 0;
    for(i = position; i < strlen(buffer); i++)
    {
        if(buffer[i] == '\"')
            break;
    }
    i++;
    for(j = 0; buffer[i + j] != '\"'; j++)
    {
        target[j] = buffer[i + j];
    }
    target[j] = '\0';
    target[j + 1] = '\0';
    return 0;
}

int cutNum(const char * menu, char *buffer)
{
    int target = 0;
    int position = findMenu(menu, buffer);
    if(position == -1)
        return -1;
    ESP_LOGI(TAG, "position %d", position);
    int state = 0;
    int i = 0;
    int j = 0;
    for(i = position; i < strlen(buffer); i++)
    {
        if(buffer[i] == ':')
            break;
    }
    i++;
    for(j = 0; buffer[i + j] != '}' && buffer[i + j] != ','; j++)
    {
        target = target * 10 + buffer[i + j] - '0';
    }
    

    return target;
}

static bool http_rest_with_url(void)
{
    /**
     * NOTE: All the configuration parameters for http_client must be spefied either in URL or as host and path parameters.
     * If host and path parameters are not set, query parameter will be ignored. In such cases,
     * query parameter should be specified in URL.
     *
     * If URL as well as host and path parameters are specified, values of host and path will be considered.
     */
        char output_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};   // Buffer to store response of http request
    int content_length = 0;
    char URL[256];
    bool success = false;
    if (city[0] == '\0')
    {
        // 未指定城市，wttr.in 自动按 IP 定位
        snprintf(URL, sizeof(URL), "https://wttr.in?format=j2&m");
    }
    else
    {
        // 指定城市，wttr.in 直接按城市名查询（支持中文城市）
        snprintf(URL, sizeof(URL), "https://wttr.in/%s?format=j2&m", city);
    }
    printf("%s", URL);
    esp_http_client_config_t config = {
        .url = URL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        /* wttr.in 要求必须带 User-Agent，否则直接返回 429 限流 */
        .user_agent = "C_charger/2.0.0",
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
   // GET Request
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    } else {
        content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0) {
            ESP_LOGE(TAG, "HTTP client fetch headers failed");
        } else {
            int data_read = esp_http_client_read_response(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
            if (data_read >= 0) {
                ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
                ESP_LOGI(TAG, "data:%s", output_buffer);
                
                // 清空旧数据
                memset(&mWeather, 0, sizeof(mWeather));

                // wttr.in j2 JSON 格式字段解析
                cutString("\"temp_C\"", mWeather.tem, output_buffer);           // 当前温度
                cutString("\"humidity\"", mWeather.humidity, output_buffer);    // 湿度
                cutString("\"value\"", mWeather.wea, output_buffer);            // 天气描述（weatherDesc[0].value）
                cutString("\"observation_time\"", mWeather.updateTime, output_buffer);  // 观测时间
                cutString("\"date\"", mWeather.date, output_buffer);            // 日期
                cutString("\"maxtempC\"", mWeather.temDay, output_buffer);      // 白天最高温
                cutString("\"mintempC\"", mWeather.temNight, output_buffer);    // 夜晚最低温

                // 城市名使用请求参数或从 API 返回中提取
                if (city[0] == '\0')
                {
                    // 未指定城市时，从 wttr.in 返回的 nearest_area 中提取市名
                    int areaPos = findMenu("\"areaName\"", output_buffer);
                    if (areaPos != -1)
                    {
                        cutString("\"value\"", mWeather.city, output_buffer + areaPos);
                    }
                }
                else
                {
                    strncpy(mWeather.city, city, sizeof(mWeather.city) - 1);
                }

                // 判定是否拿到有效结果：温度字段非空即视为成功
                if (mWeather.tem[0] != '\0') {
                    success = true;
                }
	//			cJSON_parse_task(output_buffer);
            } else {
                ESP_LOGE(TAG, "Failed to read response");
            }
        }
    }
    

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return success;
}
//WSD
static void http_rest_with_hostname_path(void)
{
    esp_http_client_config_t config = {
        .host = "httpbin.org",
        .path = "/get",
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
        .event_handler = _http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // GET
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    // POST
    const char *post_data = "field1=value1&field2=value2";
    esp_http_client_set_url(client, "/post");
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    //PUT
    esp_http_client_set_url(client, "/put");
    esp_http_client_set_method(client, HTTP_METHOD_PUT);
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP PUT Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP PUT request failed: %s", esp_err_to_name(err));
    }

    //PATCH
    esp_http_client_set_url(client, "/patch");
    esp_http_client_set_method(client, HTTP_METHOD_PATCH);
    esp_http_client_set_post_field(client, NULL, 0);
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP PATCH Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP PATCH request failed: %s", esp_err_to_name(err));
    }

    //DELETE
    esp_http_client_set_url(client, "/delete");
    esp_http_client_set_method(client, HTTP_METHOD_DELETE);
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP DELETE Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP DELETE request failed: %s", esp_err_to_name(err));
    }

    //HEAD
    esp_http_client_set_url(client, "/get");
    esp_http_client_set_method(client, HTTP_METHOD_HEAD);
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP HEAD Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP HEAD request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

#if CONFIG_ESP_HTTP_CLIENT_ENABLE_BASIC_AUTH
static void http_auth_basic(void)
{
    /**
     * Note: `max_authorization_retries` in esp_http_client_config_t
     * can be used to configure number of retry attempts to be performed
     * in case unauthorized status code is received.
     *
     * To disable authorization retries, set max_authorization_retries to -1.
     */
    esp_http_client_config_t config = {
        .url = "http://user:passwd@httpbin.org/basic-auth/user/passwd",
        .event_handler = _http_event_handler,
        .auth_type = HTTP_AUTH_TYPE_BASIC,
        .max_authorization_retries = -1,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP Basic Auth Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

static void http_auth_basic_redirect(void)
{
    esp_http_client_config_t config = {
        .url = "http://user:passwd@httpbin.org/basic-auth/user/passwd",
        .event_handler = _http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP Basic Auth redirect Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}
#endif

#if CONFIG_ESP_HTTP_CLIENT_ENABLE_DIGEST_AUTH
static void http_auth_digest(void)
{
    esp_http_client_config_t config = {
        .url = "http://user:passwd@httpbin.org/digest-auth/auth/user/passwd/MD5/never",
        .event_handler = _http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP Digest Auth Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}
#endif

static void https_with_url(void)
{
    esp_http_client_config_t config = {
        .url = "https://www.howsmyssl.com",
        .event_handler = _http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

static void https_with_hostname_path(void)
{
    esp_http_client_config_t config = {
        .host = "www.howsmyssl.com",
        .path = "/",
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = _http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

static void http_relative_redirect(void)
{
    esp_http_client_config_t config = {
        .url = "http://httpbin.org/relative-redirect/3",
        .event_handler = _http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP Relative path redirect Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

static void http_absolute_redirect(void)
{
    esp_http_client_config_t config = {
        .url = "http://httpbin.org/absolute-redirect/3",
        .event_handler = _http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP Absolute path redirect Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

static void http_redirect_to_https(void)
{
    esp_http_client_config_t config = {
        .url = "http://httpbin.org/redirect-to?url=https%3A%2F%2Fwww.howsmyssl.com",
        .event_handler = _http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP redirect to HTTPS Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}


static void http_download_chunk(void)
{
    esp_http_client_config_t config = {
        .url = "http://httpbin.org/stream-bytes/8912",
        .event_handler = _http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP chunk encoding Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

static void http_perform_as_stream_reader(void)
{
    char *buffer = malloc(MAX_HTTP_RECV_BUFFER + 1);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Cannot malloc http receive buffer");
        return;
    }
    esp_http_client_config_t config = {
        .url = "http://httpbin.org/get",
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err;
    if ((err = esp_http_client_open(client, 0)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        free(buffer);
        return;
    }
    int content_length =  esp_http_client_fetch_headers(client);
    int total_read_len = 0, read_len;
    if (total_read_len < content_length && content_length <= MAX_HTTP_RECV_BUFFER) {
        read_len = esp_http_client_read(client, buffer, content_length);
        if (read_len <= 0) {
            ESP_LOGE(TAG, "Error read data");
        }
        buffer[read_len] = 0;
        ESP_LOGD(TAG, "read_len = %d", read_len);
    }
    ESP_LOGI(TAG, "HTTP Stream reader Status = %d, content_length = %lld",
                    esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    free(buffer);
}

static void https_async(void)
{
    esp_http_client_config_t config = {
        .url = "https://postman-echo.com/post",
        .event_handler = _http_event_handler,
        .is_async = true,
        .timeout_ms = 5000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err;
    const char *post_data = "Using a Palantír requires a person with great strength of will and wisdom. The Palantíri were meant to "
                            "be used by the Dúnedain to communicate throughout the Realms in Exile. During the War of the Ring, "
                            "the Palantíri were used by many individuals. Sauron used the Ithil-stone to take advantage of the users "
                            "of the other two stones, the Orthanc-stone and Anor-stone, but was also susceptible to deception himself.";
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    while (1) {
        err = esp_http_client_perform(client);
        if (err != ESP_ERR_HTTP_EAGAIN) {
            break;
        }
    }
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

static void https_with_invalid_url(void)
{
    esp_http_client_config_t config = {
            .url = "https://not.existent.url",
            .event_handler = _http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %lld",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

/*
 *  http_native_request() demonstrates use of low level APIs to connect to a server,
 *  make a http request and read response. Event handler is not used in this case.
 *  Note: This approach should only be used in case use of low level APIs is required.
 *  The easiest way is to use esp_http_perform()
 */
static void http_native_request(void)
{
    char output_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};   // Buffer to store response of http request
    int content_length = 0;
    esp_http_client_config_t config = {
        .url = "http://httpbin.org/get",
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // GET Request
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    } else {
        content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0) {
            ESP_LOGE(TAG, "HTTP client fetch headers failed");
        } else {
            int data_read = esp_http_client_read_response(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
            if (data_read >= 0) {
                ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
                ESP_LOG_BUFFER_HEX(TAG, output_buffer, data_read);
            } else {
                ESP_LOGE(TAG, "Failed to read response");
            }
        }
    }
    esp_http_client_close(client);

    // POST Request
    const char *post_data = "{\"field1\":\"value1\"}";
    esp_http_client_set_url(client, "http://httpbin.org/post");
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    err = esp_http_client_open(client, strlen(post_data));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    } else {
        int wlen = esp_http_client_write(client, post_data, strlen(post_data));
        if (wlen < 0) {
            ESP_LOGE(TAG, "Write failed");
        }
        content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0) {
            ESP_LOGE(TAG, "HTTP client fetch headers failed");
        } else {
            int data_read = esp_http_client_read_response(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
            if (data_read >= 0) {
                ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
                ESP_LOG_BUFFER_HEX(TAG, output_buffer, strlen(output_buffer));
            } else {
                ESP_LOGE(TAG, "Failed to read response");
            }
        }
    }
    esp_http_client_cleanup(client);
}

static void http_partial_download(void)
{
    esp_http_client_config_t config = {
        .url = "https://dl.espressif.com/dl/esp-idf/ci/esp_http_client_demo.txt",
        .event_handler = _http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Download a file excluding first 10 bytes
    esp_http_client_set_header(client, "Range", "bytes=10-");
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }

    // Download last 10 bytes of a file
    esp_http_client_set_header(client, "Range", "bytes=-10");
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }

    // Download 10 bytes from 11 to 20
    esp_http_client_set_header(client, "Range", "bytes=11-20");
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

/* ================= 天气获取模块（统一重构） =================
   设计要点：
   - 单一任务 weather_task + 单一软件定时器（周期 1 分钟，自动重载）。
   - WiFi 连接成功后调用 weather_start()：立即触发首次获取，并启动周期定时器。
   - 获取失败/无数据时，下一个定时器节拍（1 分钟）自动重试，直到成功。
   - 每日 00:00：任务在节拍中检测到跨日，重置状态并开启新一轮获取。
   - 幂等与清理：weather_start() 重复调用不会创建重复任务/定时器；
     weather_stop() 通过通知位安全退出任务并删除定时器，避免资源泄漏与冲突。
   ============================================================ */

static TaskHandle_t s_weather_task = NULL;
static TimerHandle_t s_weather_timer = NULL;
static bool s_weather_ready = false;
static int s_weather_last_day = -1;

/* 任务通知位 */
#define WEATHER_EVT_FETCH  (1u << 0)   /* 触发一次获取 */
#define WEATHER_EVT_STOP   (1u << 1)   /* 退出任务 */

/* 软件定时器回调：周期到点，通知任务去获取 */
static void weather_timer_callback(TimerHandle_t xTimer)
{
    if (s_weather_task != NULL) {
        xTaskNotify(s_weather_task, WEATHER_EVT_FETCH, eSetBits);
    }
}

/* 本地时间是否已跨到新的一天（用于 00:00 重置） */
static bool weather_is_new_day(void)
{
    time_t now = time(NULL);
    if (now < 1600000000) {
        return false;   /* SNTP 尚未同步 */
    }
    struct tm t;
    localtime_r(&now, &t);
    if (s_weather_last_day < 0) {
        s_weather_last_day = t.tm_mday;
        return false;
    }
    if (t.tm_mday != s_weather_last_day) {
        s_weather_last_day = t.tm_mday;
        return true;
    }
    return false;
}

static void weather_task(void *pvParameters)
{
    (void)pvParameters;

    /* 等待 SNTP 时间同步，避免跨日判断异常 */
    while (time(NULL) < 1600000000) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    /* 连接成功后立即触发首次获取 */
    xTaskNotify(s_weather_task, WEATHER_EVT_FETCH, eSetBits);

    for (;;) {
        uint32_t bits = 0;
        xTaskNotifyWait(0, 0xFFFFFFFF, &bits, portMAX_DELAY);

        if (bits & WEATHER_EVT_STOP) {
            break;
        }
        if (!(bits & WEATHER_EVT_FETCH)) {
            continue;
        }

        /* 每日 00:00：重置状态，开启新一轮获取 */
        if (weather_is_new_day()) {
            s_weather_ready = false;
            ESP_LOGI(TAG, "Daily reset: start a new weather fetch round");
        }

        bool ok = http_rest_with_url();
        if (ok) {
            s_weather_ready = true;
            ESP_LOGI(TAG, "Weather fetched OK");
        } else {
            s_weather_ready = false;
            ESP_LOGW(TAG, "Weather fetch failed, will retry in 1 min");
        }
    }

    /* 清理：删除定时器并置空句柄，避免重复创建与资源泄漏 */
    if (s_weather_timer != NULL) {
        xTimerStop(s_weather_timer, portMAX_DELAY);
        xTimerDelete(s_weather_timer, portMAX_DELAY);
        s_weather_timer = NULL;
    }
    s_weather_task = NULL;
    vTaskDelete(NULL);
}

void weather_start(void)
{
    if (s_weather_task != NULL) {
        /* 已启动（如 WiFi 重连）：仅触发一次立即获取，避免重复创建任务/定时器 */
        xTaskNotify(s_weather_task, WEATHER_EVT_FETCH, eSetBits);
        return;
    }

    /* 创建周期定时器：每 1 分钟触发一次获取；失败重试也复用该节拍 */
    s_weather_timer = xTimerCreate("weather_timer",
                                   pdMS_TO_TICKS(WEATHER_FETCH_PERIOD_MS),
                                   pdTRUE,     /* 自动重载 */
                                   (void *)0,
                                   weather_timer_callback);
    if (s_weather_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create weather timer");
        return;
    }
    if (xTimerStart(s_weather_timer, portMAX_DELAY) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start weather timer");
        xTimerDelete(s_weather_timer, portMAX_DELAY);
        s_weather_timer = NULL;
        return;
    }

    /* 创建天气任务 */
    BaseType_t rc = xTaskCreate(&weather_task, "weather_task", 8192, NULL, 1, &s_weather_task);
    if (rc != pdPASS) {
        ESP_LOGE(TAG, "Failed to create weather task");
        xTimerStop(s_weather_timer, portMAX_DELAY);
        xTimerDelete(s_weather_timer, portMAX_DELAY);
        s_weather_timer = NULL;
        s_weather_task = NULL;
    }
}

void weather_stop(void)
{
    if (s_weather_task != NULL) {
        xTaskNotify(s_weather_task, WEATHER_EVT_STOP, eSetBits);
    }
    /* 定时器与任务资源在 weather_task 退出时统一清理 */
}

bool weather_is_ready(void)
{
    return s_weather_ready;
}

