/*
 * @Author: [LiaoZhelin]
 * @Date: 2022-05-09 21:17:30
 * @LastEditors: [LiaoZhelin]
 * @LastEditTime: 2022-05-09 22:36:39
 * @Description: 
 */
#ifndef _OTA_H_
#define _OTA_H_

/* OTA 升级界面状态 */
typedef enum {
    OTA_UI_IDLE = 0,     // 无升级
    OTA_UI_DOWNLOADING,  // 下载中
    OTA_UI_SUCCESS,      // 升级成功
    OTA_UI_FAILED         // 升级失败
} ota_ui_state_t;

/* OTA 升级界面共享状态（由 OTA 任务写入，UI 任务读取） */
typedef struct {
    ota_ui_state_t state;      // 当前状态
    int total_len;             // 已下载字节数
    int content_length;        // 固件总字节数
    int speed_kbps;            // 当前下载速率 KB/s
    char error_reason[64];     // 失败原因（格式："分类|详细说明"）
} ota_ui_status_t;

extern ota_ui_status_t g_ota_ui; // OTA 界面状态
extern int ota_ui_active;        // 是否进入 OTA 专属界面

void advanced_ota_example_task(void *pvParameter);
void simple_http_ota(void *pvParameter);
void trigger_ota_url(char *url);
void OTA_Init(void);
void setCheckVersion();
uint16_t getCheckVersion();
void getRunningVersion(char * v);
void getNewVersion(char * v);
uint16_t getIsEqu();
#endif
