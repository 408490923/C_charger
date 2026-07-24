# C_charger — 智能充电器固件

## 简介

本项目是一个基于 ESP32-C3 的多口智能充电器/桌面充电站的固件，派生自开源项目：

- [liaozhelin/yds-charger](https://github.com/liaozhelin/yds-charger)
- [ktx888/yds_charger](https://github.com/ktx888/yds_charger)

当前源码修改自 [ktx888/yds_charger](https://github.com/ktx888/yds_charger)。
感谢两位前辈以及 QQ 交流群（613826801）的群友。

> 如果你只是想了解如何烧录和使用，请直接跳到 [构建与烧录](#构建与烧录) 与 [菜单导航](#菜单导航)。
>
> 设备除 OLED 菜单外，还提供 **WebSocket 实时网页监控**（`charger_client.html`）+ **UDP 8000** 两种远程通道，详见 [远程控制与 API 接口](#远程控制与-api-接口)。

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
- **天气显示**：通过 HTTP 客户端按城市拉取天气；城市由 `city` 指令设置（默认城市见 `http_client.c`）。`http_client.c`
- **RGB 氛围灯**：`ws28xxTask` 按当前总功率在 绿 → 蓝 → 红 间做渐变，亮度由 `light`（0–100 百分比）控制，可选关闭。`task.c`
- **OLED 多级菜单**：参数设置、无线配置、系统监控、熄屏定时、关于、重启等。`menu.c`
- **Wi-Fi 配网**：
  - 支持 **ESP-TOUCH（SmartConfig）** 一键配网，凭证写入 NVS；
  - NVS 无凭证时回退到 `sdkconfig` 中 `EXAMPLE_ESP_WIFI_SSID` / `EXAMPLE_ESP_WIFI_PASS` 默认值；
  - 提供菜单内「配网」入口，超时（120 s）自动退出。`components/wifi/wifi.c`
- **OTA 升级**：支持两种实现：
  - **HTTP OTA（默认启用）**：由 `make ota <IP>` 或 `ota_url` 指令触发，设备从指定 HTTP 地址下载固件并写入另一 OTA 分区，OLED 显示下载进度与结果。`components/ota/ota.c`（`simple_http_ota`）
  - **HTTPS OTA（可选）**：基于 `esp_https_ota` 的实现也在 `ota.c` 中（`advanced_ota_example_task`），升级地址来自 `CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL`；**当前未接入 `app_main`（对应 `OTA_Init()` 已注释），默认不启用。**
- **SNTP 网络时间同步**：开机同步本地时间，用于熄屏时段调度与天气数据。`components/sntp/sntptime.c`
- **UDP 远程监控/控制**：内置 UDP 服务器（端口 **8000**），随设备联网自动启动，供 PC 上位机「Charger」软件或任意 UDP 客户端读取电量数据并下发控制指令。`main/udp_server.c`
- **WebSocket 实时网页监控**：内置 HTTP 服务器（端口 **81**，路径 `/ws`），联网后自动启动，每 **300 ms** 推送一次状态；浏览器打开 `charger_client.html` 即可免桥接直连监控并下发控制。`main/ws_server.c` + `charger_client.html`
- **熄屏定时**：可设置每天熄屏的起始/结束时间，保护 OLED 并降低功耗。

---

## 软件架构与目录结构

```
C_charger/
├── CMakeLists.txt            # 顶层 ESP-IDF 工程配置（EXTRA_COMPONENT_DIRS）
├── Makefile                  # 封装 idf.py 的便捷构建（build/flash/monitor/ota 等目标）
├── partitions.csv            # 分区表：nvs / otadata / phy_init / ota_0 / ota_1
├── sdkconfig                 # 工程配置（目标芯片、Wi-Fi、OTA 地址等）
├── sdkconfig.defaults        # 默认配置（持久化 CONFIG_HTTPD_WS_SUPPORT=y 等）
├── charger_client.html       # WebSocket 网页客户端（浏览器直连端口 81）
├── main/                     # 应用主代码
│   ├── main.c                # 入口：NVS/外设初始化、创建各 FreeRTOS 任务
│   ├── task.c / task.h       # adcTask / sw35xxTask / ws28xxTask / dht11Task 等后台任务
│   ├── menu.c / menu.h       # OLED 界面与菜单状态机、NVS 参数读写、远程控制
│   ├── http_client.c/.h      # 天气等 HTTP 数据拉取
│   ├── udp_server.c/.h       # UDP 8000 远程监控/控制服务
│   ├── ws_server.c/.h        # WebSocket 81 实时推送/控制服务
│   └── dht11.c/.h            # DHT11 温湿度驱动
└── components/               # 自定义组件 + 第三方库
    ├── wifi/                 # Wi-Fi STA + SmartConfig 配网 + NVS 存储
    ├── ota/                  # OTA 升级（HTTP 与 HTTPS 两种实现）
    ├── sw3526/               # SW3526 充电 SoC 的 I2C 驱动（读/写寄存器、功率/协议/故障）
    ├── lis3dh/               # LIS3DH 加速度计 I2C 驱动
    ├── adc_read/             # ADC 采样（A1/A2 端口电压）
    ├── sntp/                 # SNTP 时间同步
    ├── led_strip/            # RMT 驱动 WS2812（自带 RMT 时序实现）
    └── u8g2/                 # 第三方 OLED 图形库（fork，支持 ESP32 硬件 I2C）
```

任务分工（`main/task.c` / `main/ws_server.c`）：

| 任务 | 周期/触发 | 职责 |
| --- | --- | --- |
| `adcTask` | 周期 | 采样 A1/A2 电压，换算并显示 |
| `sw35xxTask` | 周期 | 通过 I2C 轮询两路 SW3526，更新电压/电流/协议/故障，计算总功率 |
| `ws28xxTask` | 周期 | 根据总功率刷新 WS2812 颜色 |
| `dht11Task` | 周期 | `boardMode=1` 时读取温湿度（其他模式跳过） |
| `udp_server_task` | 事件/周期 | 联网后启动 UDP 8000，响应状态查询与控制指令 |
| `ws_push_task` | 200 ms | 通过 WebSocket 81 周期推送状态（仅在客户端连接时） |

---

## 菜单导航

设备配有三个按键（GPIO8 = 上/返回、GPIO9 = 确认/进入、GPIO10 = 下/设置，具体映射以代码为准），长按可进入主菜单。菜单树如下：

```
主菜单
├── 参数设置
│   ├── 快充协议限制      （PPS 开关等协议限制）
│   ├── 熄屏显示输入电压  （熄屏时是否显示 VIN）
│   └── RGB/亮度设置      （通过左右键调节各通道比重与亮度）
├── 无线配置
│   ├── Wifi状态          （当前连接信息）
│   └── 配网              （启动 ESP-TOUCH 配网）
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

> 实际固件可能还包含 `boardMode` 等额外菜单项，请以 `menu.c` 为准。

参数通过 NVS 持久化（命名空间 `storage`），断电不丢失。

---

## 构建与烧录

### 依赖

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32c3/)（本项目 `sdkconfig` 基于 esp-idf 4.x；Dev 分支在适配 esp-idf 5.0.7）
- 目标芯片：**esp32c3**，Flash 4 MB

### 使用 Make（推荐，封装 idf.py）

仓库根目录 `Makefile` 封装了 `idf.py`，常用目标：

```bash
# 编译（产物 build/yds_charger.bin）
make build

# 烧录到串口（PORT 默认 /dev/cu.usbserial-*, 可在 Makefile 或环境变量覆盖）
make flash

# 烧录并打开串口监视
make monitor

# 一键 OTA：本地起 HTTP 服务(端口 8000) 提供 build/yds_charger.bin，
# 并通过 UDP 8000 向设备下发 ota_url 触发升级
make ota <设备IP>
```

> 提示：`sdkconfig.defaults` 已持久化 `CONFIG_HTTPD_WS_SUPPORT=y`，确保 WebSocket 服务可被编译进固件；执行 `make clean` / `make fullclean` 后再 `make build` 亦不会丢失该配置。

### 使用 CMake / idf.py（备选）

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
| `CONFIG_HTTPD_WS_SUPPORT=y` | 开启 HTTP 服务器 WebSocket 支持（WebSocket 服务依赖；已写入 `sdkconfig.defaults`） |
| `CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL` | 可选 HTTPS OTA 变体的升级地址（当前 HTTPS 变体未接入 `app_main`，默认不生效） |

**手机配网**：在菜单「无线配置 → 配网」中选择配网，使用支持 **ESP-TOUCH**（如 Espressif 的 ESP Touch App）的工具发送 Wi-Fi 账号密码，设备接收后保存到 NVS 并自动连接（超时 120 s 退出）。

**上位机 / 远程**：UDP 8000 与 WebSocket 81 服务均在设备联网后**自动启动**，无需在菜单中单独开启。

---

## 远程控制与 API 接口

设备提供两种远程协议，二者下发的**控制指令格式完全相同**（文本 `key="value"`），且设备对状态查询的响应体格式一致。

### 状态响应格式

任一通道收到连接后，设备会周期性（WebSocket 每 300 ms；UDP 为查询即回）返回如下文本（`charger_build_status`）：

```
C1 <W> <V> <A>
C2 <W> <V> <A>
A1 <V>  A2 <V>
VIN <V>
<light> <rgb0> <rgb1> <rgb2> <c1p> <c1pd> <c2p> <c2pd> <c1t> <c2t> <humi> <temp>
```

最后一行字段含义：

| 字段 | 含义 |
| --- | --- |
| `light` | 当前亮度（0–100，百分比） |
| `rgb0`/`rgb1`/`rgb2` | R/G/B 通道比重（0–255） |
| `c1p`/`c2p` | C1/C2 当前快充协议类型编号 |
| `c1pd`/`c2pd` | C1/C2 当前 PD 版本编号 |
| `c1t`/`c2t` | C1/C2 温度/告警状态码 |
| `humi`/`temp` | 湿度/温度（仅 `boardMode=1` 的 DHT11 版本有效，其余恒为 0） |

### 控制指令

所有指令以 `key="value"` 文本形式下发（数值为**固定 3 位十进制**，不足补零）：

| 指令 | 取值 | 说明 |
| --- | --- | --- |
| `light="NNN"` | 000–100 | 设置亮度百分比 |
| `rgb[0]="NNN"` | 000–255 | 设置红色通道比重 |
| `rgb[1]="NNN"` | 000–255 | 设置绿色通道比重 |
| `rgb[2]="NNN"` | 000–255 | 设置蓝色通道比重 |
| `city="<城市>"` | 文本 | 设置天气城市（设备端自动 URL 编码） |
| `ota_url="http://<host>/<file>.bin"` | URL | 触发 HTTP OTA 升级 |

示例：

```
light="050"
rgb[0]="020"
rgb[1]="255"
rgb[2]="000"
city="Shanghai"
ota_url="http://192.168.2.106:8000/yds_charger.bin"
```

### UDP 8000（PC 上位机）

- 设备联网后自动监听 UDP 8000。
- PC 端「Charger」软件或任意 UDP 客户端向其发送查询/指令即可读取数据或控制。
- `make ota <IP>` 即通过此端口下发 `ota_url` 指令完成升级。

### WebSocket 81（网页客户端）

- 设备联网后自动启动 HTTP 服务器，WebSocket 路径为 `/ws`。
- 浏览器打开仓库根目录的 **`charger_client.html`**，输入设备 IP（首次连接后自动缓存到浏览器 `localStorage`），点击「连接」即可。
- 连接采用 `ws://<设备IP>:81/ws`，断线后前端自动每 2 s 重连。
- **网页功能**：
  - 实时显示：C1/C2 功率/电压/电流、A1/A2 电压、输入电压 VIN、C1/C2 协议版本、C1/C2 温度告警、**设备当前亮度（只读，由设备推送）**。
  - 控制：亮度滑块（0–100）+「应用亮度」按钮，下发 `light="NNN"`。
- 说明：网页端已移除 RGB、城市、OTA URL 的可视化输入，相关功能改由指令或菜单设置。

---

## OTA 升级

### 通过 `make ota`（默认，HTTP）

```bash
make ota <设备IP>
```

流程：

1. 本机启动一个临时 HTTP 服务（端口 8000），提供 `build/yds_charger.bin`；
2. 通过 UDP 8000 向设备下发 `ota_url="http://<本机IP>:8000/yds_charger.bin"`；
3. 设备 `simple_http_ota` 从 HTTP 地址下载固件，写入当前未使用的 OTA 分区，校验通过后设置启动分区并重启；
4. OLED 上显示下载进度（KB/s）与最终结果（成功/失败原因）。

### 可选 HTTPS OTA

`ota.c` 中的 `advanced_ota_example_task` 基于 `esp_https_ota`，地址取 `CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL`。该实现当前**未接入 `app_main`**（`OTA_Init()` 处于注释状态），如需启用需自行在 `main.c` 中初始化并创建任务。

---

## 已知问题 / TODO

- `LIS3DH` 加速度计驱动已集成，但 `main.c` 中初始化默认被注释，重力感应/姿态相关功能暂未启用。
- 配网仍使用 `SC_TYPE_ESPTOUCH`（v1），相关 v2 分支代码保留但未启用。
- WebSocket 状态推送由 `ws_push_task` 每 300 ms 触发，仅在客户端连接时占用资源；若无客户端连接则不打扰主循环。
---

## License

详见仓库 `LICENSE` 文件。
