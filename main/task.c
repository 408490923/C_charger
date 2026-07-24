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

void ws28xxTask(void *pvParameters)
{
  uint32_t breathe = 0;
  uint8_t breathe_flag = 0;
  for (;;)
  {
    /* 呼吸亮度：0~150 之间往复，实现呼吸效果 */
    if (!breathe_flag)
    {
      breathe = (breathe > 150 ? 150 : breathe + 1);
      if (breathe == 150)
      {
        breathe_flag = 1;
      }
    }
    else
    {
      breathe = (breathe <= 0 ? 0 : breathe - 1);
      if (breathe == 0)
      {
        breathe_flag = 0;
      }
    }

    /* C1 / C2 实时输出功率 (W) */
    double c1P = ((double)sw35xx_c1.OutVol * 6) * ((double)sw35xx_c1.OutCur * 25 / 10) / 1000000;
    double c2P = ((double)sw35xx_c2.OutVol * 6) * ((double)sw35xx_c2.OutCur * 25 / 10) / 1000000;

    /* 充电功率 0.1W~30W 映射为颜色权重 t：0=绿(慢充) 1=红(快充)，中间平滑渐变 */
    double t1 = (c1P - 0.1) / (30.0 - 0.1);
    if (t1 < 0) { t1 = 0; }
    if (t1 > 1) { t1 = 1; }
    double t2 = (c2P - 0.1) / (30.0 - 0.1);
    if (t2 < 0) { t2 = 0; }
    if (t2 > 1) { t2 = 1; }

    for (int j = 0; j < 4; j += 1)
    {
      uint32_t r, g, b;
      if (j == 0 || j == 1)
      {
        /* C1/C2 充电状态：颜色仅由功率决定，不受用户 RGB 配色(rgbProportion)影响，
           否则红色分量会被 rgbProportion[0] 清零而永远显示绿色 */
        double r_norm = (j == 0) ? t1 : t2;
        double g_norm = 1.0 - r_norm;
        r = (uint32_t)(r_norm * 255 * breathe / 150 * light / 100 * rgbOn[j]);
        g = (uint32_t)(g_norm * 255 * breathe / 150 * light / 100 * rgbOn[j]);
        b = 0;
      }
      else
      {
        /* j==2/3 端口指示灯：沿用原白光呼吸逻辑（受用户 RGB 配色控制） */
        r = (uint32_t)(breathe * rgbProportion[0] * light / 100 / 150 * rgbOn[j]);
        g = (uint32_t)(breathe * rgbProportion[1] * light / 100 / 150 * rgbOn[j]);
        b = (uint32_t)(breathe * rgbProportion[2] * light / 100 / 150 * rgbOn[j]);
      }
      ESP_ERROR_CHECK(strip->set_pixel(strip, j, r, g, b));
    }
    ESP_ERROR_CHECK(strip->refresh(strip, 100));

    vTaskDelay(pdMS_TO_TICKS(10));
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
