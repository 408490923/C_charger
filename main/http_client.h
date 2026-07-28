#include <stdio.h>
#include <stdbool.h>

/* 天气获取配置 */
#define WEATHER_FETCH_PERIOD_MS 60000   /* 获取/重试周期：每 1 分钟一次 */

/* 天气获取公共接口 */
void weather_start(void);   /* WiFi 连接成功后调用：启动天气获取（幂等，可重复调用） */
void weather_stop(void);    /* WiFi 断开时调用：停止并清理天气任务（可选） */
bool weather_is_ready(void);/* 当前是否已成功获取到天气数据 */

/* 解析辅助（供其他模块复用，保持兼容） */
int cutString(const char * menu, char *target, char *buffer);
int cutNum(const char * menu, char *buffer);