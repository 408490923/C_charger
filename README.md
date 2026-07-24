# C_charger — 智能充电器固件

## 简介

本项目是一个基于 ESP32-C3 的多口智能充电器/桌面充电站的固件，派生自开源项目：

- [liaozhelin/yds-charger](https://github.com/liaozhelin/yds-charger)
- [ktx888/yds_charger](https://github.com/ktx888/yds_charger)

当前源码修改自 [ktx888/yds_charger](https://github.com/ktx888/yds_charger)。
感谢两位前辈以及 QQ 交流群（613826801）的群友。

> 如果你只是想了解如何烧录和使用，请直接跳到 [构建与烧录](#构建与烧录) 与 [菜单导航](#菜单导航)。

---

## 硬件平台

| 模块 | 说明 |
| --- | --- |
| 主控 | **ESP32-C3**（RISC-V 32 位，板载 4 MB Flash，通过 `partitions.csv` 划分 OTA 双分区） |
| 充电 SoC | **SW3526 × 2**（USB-C1 / USB-C2），通过 I2C 读取电压、电流、快充协议、故障码 |
| USB-A 口 | A1 / A2 两路，通过 **ADC** 读取端口电压 |
| 显示屏 | **SSD1306** OLED（128×64），I2C 接口，使用 `u8g2` 图形库绘制 |
| 氛围灯 | **WS2812** RGB LED × 4（GPIO4，RMT 驱动），按总功率做颜色渐变 |
| 环境传感器 | **DHT11**（温湿度，仅在 `boardMode=1` 的带传感器版本上启用） |
| 姿态传感器 | **LIS3DH** 三轴加速度计（驱动已集成，`main.c` 中初始化默认注释关闭） |
| 按键 | 3 个实体按键，GPIO **8 / 9 / 10** |
| 输出控制 | 输出使能 GPIO **20 / 21** |

---

## 功能特性

- **四口实时电量监控**：C1、C2（SW3526）、A1、A2（ADC）以及输入电压 VIN、总功率的计算与显示。
- **快充协议识别显示**：QC2.0/3.0、FCP、SCP、PD FIX、PD PPS、PE、VOOC、SFCP、AFC 等协议类型展示。
- **故障码显示**：SW3526 上报的异常状态（过压/过流/过温等）以「异常代码」界面呈现。
- **天气显示**：通过 HTTP 客户端拉取天气（基于 IP 定位）。`http_client.c`
- **RGB 氛围灯**：`ws28xxTask` 按当前总功率在 绿 → 蓝 → 红 间做渐变，可选关闭。`task.c`
- **OLED 多级菜单**：参数设置、无线配置、系统监控、熄屏定时、关于、重启等。`menu.c`
- **Wi-Fi 配网**：
  - 支持 **ESP-TOUCH（SmartConfig）** 一键配网，凭证写入 NVS；
  - NVS 无凭证时回退到 `sdkconfig` 中 `EXAMPLE_ESP_WIFI_SSID` / `EXAMPLE_ESP_WIFI_PASS` 默认值；
  - 提供菜单内「配网」入口，超时（120 s）自动退出。`components/wifi/wifi.c`
- **HTTPS OTA 升级**：基于 `esp_https_ota`，带下载进度显示与失败回退到上一分区；升级地址来自 `CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL`。`components/ota/ota.c`
- **SNTP 网络时间同步**：开机同步本地时间，用于熄屏时段调度与天气数据。`components/sntp/sntptime.c`
- **UDP 远程监控/控制**：内置 UDP 服务器（端口 **8000**），供 PC 上位机「Charger」软件实时读取电量数据并下发控制指令。`main/udp_server.c` + `menu.c` 的 `remoteControl()`
- **熄屏定时**：可设置每天熄屏的起始/结束时间，保护 OLED 并降低功耗。

---

## 软件架构与目录结构

```
C_charger/
├── CMakeLists.txt            # 顶层 ESP-IDF 工程配置（EXTRA_COMPONENT_DIRS）
├── Makefile                  # 兼容旧版 esp-idf make 构建（与 CMake 二选一）
├── partitions.csv            # 分区表：nvs / otadata / phy_init / ota_0 / ota_1
├── sdkconfig                 # 工程配置（目标芯片、Wi-Fi、OTA 地址等）
├── main/                     # 应用主代码
│   ├── main.c                # 入口：NVS/外设初始化、创建各 FreeRTOS 任务
│   ├── task.c / task.h       # adcTask / sw35xxTask / ws28xxTask / dht11Task 等后台任务
│   ├── menu.c / menu.h       # OLED 界面与菜单状态机、NVS 参数读写、远程控制
│   ├── http_client.c/.h      # 天气等 HTTP 数据拉取
│   ├── udp_server.c          # UDP 8000 远程监控/控制服务
│   └── dht11.c/.h            # DHT11 温湿度驱动
└── components/               # 自定义组件 + 第三方库
    ├── wifi/                 # Wi-Fi STA + SmartConfig 配网 + NVS 存储
    ├── ota/                  # HTTPS OTA 升级
    ├── sw3526/               # SW3526 充电 SoC 的 I2C 驱动（读/写寄存器、功率/协议/故障）
    ├── lis3dh/               # LIS3DH 加速度计 I2C 驱动
    ├── adc_read/             # ADC 采样（A1/A2 端口电压）
    ├── sntp/                 # SNTP 时间同步
    ├── led_strip/            # RMT 驱动 WS2812（自带 RMT 时序实现）
    └── u8g2/                 # 第三方 OLED 图形库（fork，支持 ESP32 硬件 I2C）
```

任务分工（`main/task.c`）：

| 任务 | 周期/触发 | 职责 |
| --- | --- | --- |
| `adcTask` | 周期 | 采样 A1/A2 电压，换算并显示 |
| `sw35xxTask` | 周期 | 通过 I2C 轮询两路 SW3526，更新电压/电流/协议/故障，计算总功率 |
| `ws28xxTask` | 周期 | 根据总功率刷新 WS2812 颜色 |
| `dht11Task` | 周期 | `boardMode=1` 时读取温湿度（其他模式跳过） |

---

## 菜单导航

设备配有三个按键（GPIO8 = 上/返回、GPIO9 = 确认/进入、GPIO10 = 下/设置，具体映射以代码为准），长按可进入主菜单。菜单树如下：

```
主菜单
├── 参数设置
│   ├── 快充协议限制      （PPS 开关等协议限制）
│   └── 熄屏显示输入电压  （熄屏时是否显示 VIN）
├── 无线配置
│   ├── Wifi状态          （当前连接信息）
│   ├── 配网              （启动 ESP-TOUCH 配网）
├── 系统监控
│   ├── 电压电流          （各口实时数据）
│   ├── 快充协议          （当前识别到的协议）
│   └── 异常代码          （SW3526 故障码）
├── 设置熄屏时间
│   ├── 起始时间
│   └── 结束时间
├── 关于
│   └── 版本信息          （固件版本）
└── 重启
```

参数通过 NVS 持久化（命名空间 `chargerConfig`），断电不丢失。

---

## 构建与烧录

### 依赖

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32c3/)（本项目 `sdkconfig` 基于 esp-idf 4.x；Dev 分支在适配 esp-idf 5.0.7）
- 目标芯片：**esp32c3**，Flash 4 MB

### 使用 CMake（推荐）

```bash
# 设置 IDF 环境（根据你的安装路径）
. $IDF_PATH/export.sh

# 指定目标芯片
idf.py set-target esp32c3

# （可选）按需修改配置
idf.py menuconfig

# 编译
idf.py build

# 烧录并监视（将 <PORT> 替换为实际串口，如 /dev/ttyUSB0）
idf.py -p <PORT> flash monitor
```

### 使用旧版 Make

仓库同时提供了 `Makefile`，可配合旧版 esp-idf（`make` / `make flash monitor`）构建，二选一即可。

### 分区说明

`partitions.csv` 采用 OTA 双 app 分区方案（无 factory 分区）：

```
nvs       0x9000   0x4000
otadata   0xd000   0x2000
phy_init  0xf000   0x1000
ota_0     0x10000  0x1C0000
ota_1     0x1D0000 0x1E0000
```

这意味着首次烧录需写入 `otadata` 以选定 `ota_0`；OTA 升级会写到另一分区并切换。

---

## 配置项

下列配置位于 `sdkconfig`（可用 `idf.py menuconfig` 修改）：

| 配置项 | 说明 |
| --- | --- |
| `CONFIG_IDF_TARGET="esp32c3"` | 目标芯片 |
| `EXAMPLE_ESP_WIFI_SSID` / `EXAMPLE_ESP_WIFI_PASS` | NVS 无凭证时的默认 Wi-Fi 账号密码 |
| `EXAMPLE_ESP_MAXIMUM_RETRY` | Wi-Fi 连接最大重试次数（超出后触发配网） |
| `CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL` | HTTPS OTA 升级固件地址 |

**手机配网**：在菜单「无线配置 → 配网」中选择配网，使用支持 **ESP-TOUCH**（如 Espressif 的 ESP Touch App）的工具发送 Wi-Fi 账号密码，设备接收后保存到 NVS 并自动连接（超时 120 s 退出）。

**上位机**：PC 端「Charger」软件通过 UDP 8000 端口连接设备，需在菜单「无线配置 → 远程控制」中开启。

---

## 已知问题 / TODO

- `LIS3DH` 加速度计驱动已集成，但 `main.c` 中初始化默认被注释，重力感应/姿态相关功能暂未启用。
- 配网仍使用 `SC_TYPE_ESPTOUCH`（v1），相关 v2 分支代码保留但未启用。
---

## License

详见仓库 `LICENSE` 文件。
