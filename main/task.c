/*
 * @Author: [LiaoZhelin]
 * @Date: 2022-05-10 14:35:47
 * @LastEditors: [Zyilin98]
 * @LastEditTime: 2025-01-03 18:07:43
 * @Description: 
 */
#include "task.h"
#include <stdio.h>
#include <string.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "adc_read.h"
#include "lis3dh.h"
//#include "led_strip.h"
#include "sw3526.h"
#include "dht11.h"
#include "menu.h"
static const char *TAG = "task";
extern int16_t rgbProportion[3];
extern int16_t light;
int8_t rgbOn[4] = {0};

void dht11Task(void *pvParameters)
{
  for(;;)
  {
     DHT11();

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  vTaskDelete(NULL);
}

void adcTask(void *pvParameters)
{
  for (;;)
  {
    ADC_getVoltage(ADC);
    if(aPortLed == 0)
    {
      if(OledProtectBegin == 0)
      {
        rgbOn[3] = 1;
        rgbOn[2] = 1;
      }
      else
      {
      rgbOn[3] = ADC[0] / 1000 > 6 ? 1 : 0;
      rgbOn[2] = ADC[1] / 1000 > 6 ? 1 : 0;
      }
    }
    else
    {
      rgbOn[3] = 1;
      rgbOn[2] = 1;
    }

    vTaskDelay(pdMS_TO_TICKS(200));
  }
  vTaskDelete(NULL);
}
void sw35xxTask(void *pvParameters)
{
  for (;;)
  {
    
    SW35XXUpdate();
    double c1P = ((double)sw35xx_c1.OutVol * 6) * ((double)sw35xx_c1.OutCur * 25 / 10) / 1000000;
    double c2P = ((double)sw35xx_c2.OutVol * 6) * ((double)sw35xx_c2.OutCur * 25 / 10) / 1000000;
    if(c1P > 0.2)
    {
      rgbOn[0] = 1;
    }
    else if(c1P < 0.1)
    {
      rgbOn[0] = 0;
    }


    if(c2P > 0.2)
    {
      rgbOn[1] = 1;
    }
    else if(c2P < 0.1)
    {
      rgbOn[1] = 0;
    }
    c1Power += (((double)sw35xx_c1.OutVol * 6) / 1000 * ((double)sw35xx_c1.OutCur * 25 / 10) / 1000) / 18000;
    c2Power += (((double)sw35xx_c2.OutVol * 6) / 1000 * ((double)sw35xx_c2.OutCur * 25 / 10) / 1000) / 18000;
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

/* 功率(W) -> 归一化 RGB 颜色权重：
   0.1W~20W  绿(0,1,0) 平滑过渡到 蓝(0,0,1)
   20W~65W   蓝(0,0,1) 平滑过渡到 红(1,0,0)
   两段在 20W 处均为纯蓝，保证渐变连续 */
static void powerToColor(double power, double *r, double *g, double *b)
{
    if (power < 0.1) power = 0.1;
    if (power > 65.0) power = 65.0;

    if (power <= 20.0)
    {
        double t = (power - 0.1) / (20.0 - 0.1); /* 0=绿 1=蓝 */
        *r = 0.0;
        *g = 1.0 - t;
        *b = t;
    }
    else
    {
        double t = (power - 20.0) / (65.0 - 20.0); /* 0=蓝 1=红 */
        *r = t;
        *g = 0.0;
        *b = 1.0 - t;
    }
} 


void ws28xxTask(void *pvParameters)
{
  /* 常亮模式：取消呼吸，亮度固定为满（breathe 恒为 200，使 breathe/200 = 1） */
  uint32_t breathe = 200;
  for (;;)
  {
    /* C1 / C2 实时输出功率 (W) */
    double c1P = ((double)sw35xx_c1.OutVol * 6) * ((double)sw35xx_c1.OutCur * 25 / 10) / 1000000;
    double c2P = ((double)sw35xx_c2.OutVol * 6) * ((double)sw35xx_c2.OutCur * 25 / 10) / 1000000;

    for (int j = 0; j < 4; j += 1)
    {
      uint32_t r, g, b;
      if (j == 0 || j == 1)
      {
        /* C1/C2 充电状态：颜色仅由功率决定，不受用户 RGB 配色(rgbProportion)影响 */
        double r_norm, g_norm, b_norm;
        powerToColor((j == 0) ? c1P : c2P, &r_norm, &g_norm, &b_norm);
        r = (uint32_t)(r_norm * 255 * breathe / 200 * light / 100 * rgbOn[j]);
        g = (uint32_t)(g_norm * 255 * breathe / 200 * light / 100 * rgbOn[j]);
        b = (uint32_t)(b_norm * 255 * breathe / 200 * light / 100 * rgbOn[j]);
      }
      else
      {
        /* j==2/3 端口指示灯：沿用原白光呼吸逻辑（受用户 RGB 配色控制） */
        r = (uint32_t)(breathe * rgbProportion[0] * light / 100 / 200 * rgbOn[j]);
        g = (uint32_t)(breathe * rgbProportion[1] * light / 100 / 200 * rgbOn[j]);
        b = (uint32_t)(breathe * rgbProportion[2] * light / 100 / 200 * rgbOn[j]);
      }
      ESP_ERROR_CHECK(strip->set_pixel(strip, j, r, g, b));
    }
    ESP_ERROR_CHECK(strip->refresh(strip, 100));

    vTaskDelay(pdMS_TO_TICKS(2.5));
  }
}

/*void lis3dhTask(void *pvParameters)
{
  uint8_t buffer1,buffer2;
  uint16_t X_V,Y_V,Z_V;
  for (;;)
  {
    LIS3DH_ReadReg(LIS3DH_REG_OUT_X_L,&buffer1);
    LIS3DH_ReadReg(LIS3DH_REG_OUT_X_H,&buffer2);
    X_V = ((buffer2<<8)|buffer1);

    LIS3DH_ReadReg(LIS3DH_REG_OUT_Y_L,&buffer1);
    LIS3DH_ReadReg(LIS3DH_REG_OUT_Y_H,&buffer2);
    Y_V = ((buffer2<<8)|buffer1);
    LIS3DH_ReadReg(LIS3DH_REG_OUT_Z_L,&buffer1);
    LIS3DH_ReadReg(LIS3DH_REG_OUT_Z_H,&buffer2);
    Z_V = ((buffer2<<8)|buffer1);
    //ESP_LOGI(TAG, "LIS3DH_X=%d  LIS3DH_Y=%d  LIS3DH_Z=%d",X_V,Y_V,Z_V);
    vTaskDelay(pdMS_TO_TICKS(40));
  }
}
*/
/*void ntpClockTask(void *pvParameters){
  for (;;){
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
*/
void taskMonitor(void *pvParameters){
  UBaseType_t uxHighWaterMark;
  for (;;){
    vTaskDelay(pdMS_TO_TICKS(1000));
    printf("--------------------------------------------\r\n");
    uxHighWaterMark = uxTaskGetStackHighWaterMark(adcTask_handle);
    printf("Task: adcTask_handle stacksize=%d\r\n",uxHighWaterMark);

    uxHighWaterMark = uxTaskGetStackHighWaterMark(sw35xxTask_handle);
    printf("Task: sw35xxTask_handle stacksize=%d\r\n",uxHighWaterMark);

    uxHighWaterMark = uxTaskGetStackHighWaterMark(ws28xxTask_handle);
    printf("Task: ws28xxTask_handle stacksize=%d\r\n",uxHighWaterMark);

    /*uxHighWaterMark = uxTaskGetStackHighWaterMark(lis3dhtask_handle);
    printf("Task: lis3dhtask_handle stacksize=%d\r\n",uxHighWaterMark);
*/
    uxHighWaterMark = uxTaskGetStackHighWaterMark(oledTask_handle);
    printf("Task: oledTask_handle stacksize=%d\r\n",uxHighWaterMark);

    uxHighWaterMark = uxTaskGetStackHighWaterMark(ntpTask_handle);
    printf("Task: ntpTask_handle stacksize=%d\r\n",uxHighWaterMark);
  }
}
