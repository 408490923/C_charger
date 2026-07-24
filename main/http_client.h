#include <stdio.h>

/* 天气刷新参数（供 main.c / http_client.c 共用） */
#define WEATHER_INIT_DELAY_MS 5000   /* Wi-Fi 连接成功后首次刷新延时 */
#define WEATHER_RETRY_COUNT   20     /* 失败时最大重试次数 */
#define WEATHER_RETRY_DELAY_MS 5000  /* 每次重试间隔 */

void http_test_task(void *pvParameters);
void weather_daily_task(void *pvParameters);
int cutString(const char * menu, char *target, char *buffer);
int cutNum(const char * menu, char *buffer);