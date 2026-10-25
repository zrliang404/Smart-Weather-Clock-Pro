# 智能天气时钟升级版（STM32F103 + ESP32 + ST7735）（原作者：b站博主梅花七月香）

一个基于 **STM32F103C8T6** 的桌面天气时钟：通过 **ESP32（ESP-AT 固件）** 联网，从心知天气（Seniverse）获取实时天气、用 SNTP 校准时间，在 **1.44 寸 128×128 ST7735 彩屏** 上显示时间、日期、星期、天气图标、气温和板载 NTC 测得的室温，并支持多城市切换与蜂鸣器闹钟。

> 本项目为学习/DIY 用途的裸机（裸金属）工程，使用 STM32 标准外设库（StdPeriph）开发，IDE 为 Keil MDK 5。

---

## ✨ 功能特性

- **网络对时**：开机与每小时通过 ESP-AT 的 SNTP 功能同步时间，写入 STM32 内置 RTC（LSE 32.768 kHz + 后备域，带纽扣电池可掉电走时）。
- **实时天气**：调用心知天气 `weather/now.json` 接口，解析城市、天气现象、温度、更新时间。
- **多城市**：内置 5 个城市（肇庆、上海、广州、北京、黑河），按键循环切换；每个城市单独缓存，**缓存有效期 10 分钟**。
- **后台预取**：主循环每 3 秒检查一次，优先刷新「当前城市 → 下一个城市 → 上一个城市」中最早过期的一个，切换城市时基本无需等待。
- **天气图标**：8 张 32×32 RGB565 图标（晴、多云、阴、雨、雷雨、雪、雾、风），按接口返回的英文天气描述匹配。
- **温度配色**：气温与室温文字颜色随温度在蓝→红之间渐变（`temperature_to_color_gradient`）。
- **板载测温**：PA0 上的 10 kΩ NTC（B=3950）经 ADC1 + DMA 多次采样取平均，再换算为摄氏度。
- **闹钟**：独立设置页，可调时、分、开关；到点后蜂鸣器鸣叫并全屏红黑爆闪 `WAKE UP!`，最长 3 分钟后自动停止，按 PA11 可手动停止。
- **开机自检界面**：逐行打印 ESP32 / WiFi / SNTP 初始化状态，并用进度条显示 5 个城市的首次天气拉取进度；任一步失败会在屏幕上显示 `Failed!!!`。

---

## 🧩 硬件组成

| 模块 | 型号 / 规格 | 说明 |
| --- | --- | --- |
| 主控 | STM32F103C8T6 | Cortex-M3，72 MHz，64 KB Flash / 20 KB RAM |
| 显示屏 | ST7735 1.44" TFT，128×128，SPI | RGB565，DMA 发送 |
| 联网 | ESP32 模组（烧录 **ESP-AT** 固件） | 串口 AT 指令，需支持 `AT+HTTPCGET` |
| 时钟 | STM32 内部 RTC + LSE 32.768 kHz 晶振 | VBAT 接纽扣电池可掉电走时 |
| 测温 | 10 kΩ NTC（B=3950）+ 10 kΩ 分压 | 接 ADC1_IN0 |
| 交互 | 4 个按键、1 个蜂鸣器、1 个 LED | 见下表 |
| 预留 | MPU6050（软件 I2C） | 驱动已包含，主程序暂未使用 |

### 引脚连接

| 功能 | STM32 引脚 | 备注 |
| --- | --- | --- |
| LCD SPI1_SCK | PA5 | 硬件 SPI1，模式 3，18 MHz |
| LCD SPI1_MOSI | PA7 | 只写，无需 MISO |
| LCD CS | PA4 | 软件片选 |
| LCD DC | PB1 | 数据/命令 |
| LCD RESET | PB0 | 复位 |
| LCD BLK | PB10 | 背光，高电平点亮 |
| ESP32 UART TX | PA2 (USART2_TX) | → ESP32 RX |
| ESP32 UART RX | PA3 (USART2_RX) | ← ESP32 TX，115200 8N1 |
| NTC | PA0 (ADC1_IN0) | 10 kΩ 上拉到 3.3 V |
| 蜂鸣器 | PB8 | **低电平触发** |
| KEY0 | PB12 | EXTI12，下降沿 |
| KEY1 | PB14 | EXTI14，下降沿 |
| KEY2 | PA11 | 轮询，下降沿有效 |
| WK_UP | PA9 | 轮询，下降沿有效 |
| LED | PC13 | 低电平点亮 |
| RTC 晶振 | PC14 / PC15 | 32.768 kHz |
| SWD 下载 | PA13 / PA14 | ST-Link |

> ESP32 与 STM32 必须共地，ESP32 使用 3.3 V 供电（注意峰值电流）。

---

## 🗂 目录结构

```
.
├── app/                     # 应用层
│   ├── main.c               # 主循环、UI 状态机、闹钟、天气缓存
│   ├── board.c              # 板级时钟/外设时钟、LSE 启动
│   ├── delay.c              # 微秒/毫秒延时（SysTick）
│   ├── weather.c/.h         # 心知天气 JSON 字段解析
│   ├── font_*.c             # 数字/中文字模
│   └── icon/                # 8 张天气图标（RGB565）
├── driver/                  # 外设驱动
│   ├── st7735/              # LCD 驱动、SPI+DMA、ASCII 字库、图片结构
│   ├── esp-at/              # ESP-AT 指令封装、USART2 中断收发
│   ├── mpu6050/             # 六轴传感器 + 软件 I2C（未启用）
│   ├── ntc/                 # ADC + DMA 采样
│   ├── rtc.c / timer.c      # RTC 时间戳换算、1 ms 软定时器
│   └── led.c
├── HARDWARE/                # 按键、外部中断、位带宏
├── firmware/                # CMSIS + STM32F10x 标准外设库
├── mdk/                     # Keil MDK 工程（stm32f103.uvprojx）
└── .vscode/                 # VS Code IntelliSense 配置
```

## ⚙️ 工作流程

```
上电
 ├─ 板级/外设初始化（时钟、NTC、LED、按键、蜂鸣器、RTC、EXTI、LCD）
 ├─ ESP-AT 初始化：AT+RESTORE → ATE0 → AT+SYSSTORE=0
 ├─ WiFi：AT+CWMODE=1 → AT+CWJAP="ssid","pwd"
 ├─ SNTP：AT+CIPSNTPCFG=1,8,"cn.ntp.org.cn","ntp.sjtu.edu.cn"
 ├─ 首次拉取 5 个城市天气（进度条）
 ├─ 读取时间戳 → rtc_set_timestamp(ts + 8h)
 └─ 主循环
      ├─ 按键路由：主界面切换城市；PA9 进入/退出闹钟页
      ├─ 闹钟响铃：蜂鸣器 + 屏幕爆闪，最长 180 s
      ├─ 每 100 ms：刷新时间、判断闹钟触发
      ├─ 每 1 s：刷新 NTC 室温
      ├─ 每 3 s：后台预取最早过期的城市天气
      └─ 每小时：重新 SNTP 对时
```

---

## 🔨 编译与烧录

**环境**

- Keil MDK-ARM 5.x（ARMCC v5 编译器）
- 器件支持包：`Keil.STM32F1xx_DFP` 2.2.0
- 下载器：ST-Link V2（工程默认调试器为 ST-Link）

**步骤**

1. 用 Keil 打开 `mdk/stm32f103.uvprojx`。
2. 确认 `Options for Target → C/C++` 中宏定义包含
   `STM32F10X_MD, USE_STDPERIPH_DRIVER`，
   头文件路径包含 `..\app;..\driver;..\firmware\cmsis\core;..\firmware\cmsis\device;..\firmware\driver\inc;..\driver\st7735;..\driver\esp-at;..\driver\mpu6050;..\HARDWARE\KEY;..\HARDWARE\EXTI;..\HARDWARE\sys;..\driver\ntc`
   （工程内已配置好）。
3. `Build` 编译，`Download` 烧录。
4. 若需用其它工具烧录，请在 `Options for Target → Output` 勾选 **Create HEX File**（工程默认未勾选）。

**ESP32 侧**

- 烧录支持 `AT+HTTPCGET` 的 ESP-AT 固件，默认波特率 115200。
- 程序每次启动都会执行 `AT+RESTORE` 恢复出厂设置，因此 **WiFi 信息不会保存在 ESP32 里**，而是每次由 STM32 下发。

---

## 🔑 配置说明

联网参数与天气 API Key 位于 `app/main.c` 顶部（**开源前务必替换为你自己的**）：

```c
#define WEATHER_URL(city) "https://api.seniverse.com/v3/weather/now.json?key=<你的API_KEY>&location=" #city "&language=en&unit=c"

static const char *wifi_ssid = "<你的WiFi名称>";
static const char *wifi_password = "<你的WiFi密码>";
```

- 心知天气免费版 Key 有调用频率限制（约 20 次/分钟、400 次/天），本项目每 3 秒检查一次、每城市 10 分钟过期，正常使用不会超限。
- `location` 使用拼音（`zhaoqing`、`shanghai`…），新增城市需同时准备对应的 24×24 中文字模和 `cities[]` 表项。
- 时区在 `AT+CIPSNTPCFG=1,8,...` 与 `rtc_set_timestamp(ts + 8*60*60)` 中硬编码为 UTC+8，其它时区需自行修改。

---

## 🎮 按键操作

| 界面 | 按键 | 功能 |
| --- | --- | --- |
| 主界面 | KEY0 (PB12) | 下一个城市 |
| 主界面 | KEY1 (PB14) | 上一个城市 |
| 主界面 | PA11 | 停止响铃（响铃时） |
| 全局 | WK_UP (PA9) | 进入 / 退出闹钟设置页 |
| 闹钟页 | PA11 | 切换设置项（时 / 分 / 开关） |
| 闹钟页 | KEY0 / KEY1 | 当前项 +1 / −1 |

---

## ⚠️ 已知限制

- 仅在特定 ESP-AT 固件版本上验证，不同版本 AT 指令（如 `AT+HTTPCGET`）可能不同。
- 天气 JSON 采用**字符串匹配**解析，而非完整 JSON 解析器，依赖服务端字段格式稳定。
- 主循环为阻塞式：初始化失败会 `while(1)` 卡死，AT 指令期间屏幕与按键无响应；未启用独立看门狗。
- 闹钟时间保存在 RAM，**掉电丢失**，尚未写入 BKP 寄存器或 Flash。
- 屏幕偏移量（`ST7735_XSTART/YSTART`）与驱动 IC 批次有关，换屏后可能需要调整。
- 城市列表、时区、字体均为编译期固定，暂不支持配网/配置持久化。

## 🚧 TODO

- [ ] 把 WiFi / API Key 抽到不提交的 `user_config.h`
- [ ] 闹钟参数写入 BKP/Flash 掉电保存
- [ ] 加入 IWDG 与网络失败重试
- [ ] 支持配置模式（AP 配网 / 串口配置）
- [ ] 用 MPU6050 实现抬腕亮屏

---

## 📄 开源协议

版权归 **梅花嵌入式**，本项目在其教学代码基础上二次开发，`firmware/` 下为 ST 的 CMSIS 与标准外设库（BSD-3-Clause 风格授权。

建议整体以 **MIT License** 开源，并：

1. 新增 `LICENSE` 文件（MIT 全文，版权人填你自己）；
2. 若代码来源于课程/教程模板，请先确认其许可条款允许公开再发布。

## 🙏 致谢

- [心知天气 Seniverse](https://www.seniverse.com/)：天气数据接口
- 梅花嵌入式：STM32 教学代码与 ST7735 驱动（b站链接:【【全开源】stm32智能天气时钟,从零打造！(可写简历)】 https://www.bilibili.com/video/BV12hmFYBEVc/?share_source=copy_web&vd_source=82b917c7988999df0cca0f951ebeab53）
- STMicroelectronics：CMSIS、STM32F10x 标准外设库
