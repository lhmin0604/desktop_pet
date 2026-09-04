# 🐾 桌上宠物项目 — 交付文档

> **用途**：将此文档提供给新的 AI 对话窗口，可快速理解项目全貌并继续推进开发。
> **最后更新**：2026-09-03（硬件改为 BOX-3B+DOCK；新增屏幕表情架构；语音改用板载麦克风+ESP-SR）
> **当前阶段**：Phase 1 框架代码完成，待硬件联调；屏幕表情/语音架构已设计，待联调后实现

---

## 一、项目概述

### 目标
用 **STC-B 学习板**（STC15F2K60S2, 8051内核）和 **ESP32-S3-BOX-3B + BOX-3-DOCK** 合作制作一个**桌上宠物**。
- **ESP32-S3-BOX-3B** = 🧠大脑 + 😺脸 + 👂耳朵 + 🔊嘴：宠物状态机、**2.4寸彩屏表情**、**板载麦克风语音识别**、**板载喇叭音效**、联网智能
- **STC-B** = 🖐身体 + 💡氛围：传感器感知、按键交互、LED 氛围灯、蜂鸣器物理音效、步进电机动做、数码管辅助信息

### 项目路径
```
TEXT/DesktopPet/
```

### 文件清单

```
DesktopPet/
├── README.md                          # 项目总览、接线图、开发路线图
├── QUICKSTART.md                      # 快速上手指南（接线→烧录→联调→测试）
├── HANDOFF.md                         # 本文档（交付文档）
│
├── protocol/
│   ├── 通信协议.md                     # STC-B ↔ ESP32 串口通信协议规范 V1.0
│   ├── ASRPRO语音模块接口协议.md        # ASRPRO 2.0 接口协议（已弃用，改由 BOX-3B 板载麦克风替代）
│   └── 屏幕显示架构.md                 # BOX-3B 屏幕表情系统架构（像素复古风+触摸+语音）
│
├── STC-B/                             # Keil C51 工程（STC-B 固件）
│   ├── inc/
│   │   ├── comm.h                     # 通信模块：帧常量、命令码、表情/音效/事件 ID
│   │   └── expression.h               # 表情引擎：接口声明
│   └── source/
│       ├── main.c                     # 主程序：初始化 + 事件回调注册 + 命令分发
│       ├── comm.c                     # 通信实现：UART2 状态机帧解析 + 发送
│       └── expression.c               # 表情实现：12种表情段码表 + 8种音效 + LED
│
└── ESP32/                             # Arduino 工程（ESP32-S3 固件）
    ├── include/
    │   ├── PetProtocol.h              # 协议处理器：帧解析 + 命令发送 + 回调注册
    │   └── PetState.h                 # 宠物状态机：情绪枚举、属性结构体、接口
    └── src/
        ├── desktop_pet.ino            # Arduino 主程序：setup/loop + 定时器
        ├── PetProtocol.cpp            # 协议处理器实现
        └── PetState.cpp               # 宠物状态机实现（大脑核心逻辑）
```

---

## 二、硬件平台信息

### STC-B 学习板
- **MCU**: STC15F2K60S2（增强型 8051 内核）
- **系统时钟**: 11,059,200 Hz（`code unsigned long SysClock = 11059200;`）
- **开发环境**: Keil C51 v9.51a
- **烧录工具**: STC-ISP v6.88F
- **串口芯片**: CH340（USB 转串口，UART1 固定）
- **BSP 框架版本**: V3.6（协作式事件驱动框架）

### STC-B 板载外设（全部已驱动）
| 外设 | 模块 | 头文件 | 关键说明 |
|------|------|--------|---------|
| 8位数码管 | 动态扫描，共阴极 | displayer.h V2.0a | `Seg7Print(d0..d7)` + `LedPrint(led)` |
| 8个LED | L0-L7 指示灯 | displayer.h | 位掩码控制 |
| 3个按键 | K1/K2/K3 | Key.h V2.0 | K3 与 ADC 导航共用 P1.7 |
| 5方向导航键 | 上/下/左/右/中 | adc.h V3.5a | 通过 ADC 电阻分压识别 |
| 蜂鸣器 | CCP 通道1 | Beep.h V2.0 | `SetBeep(freq_hz, time)` |
| 温度传感器 | 10K/3950 NTC | adc.h | ADC 通道 Rt（10bit） |
| 光照传感器 | GL5516 光敏电阻 | adc.h | ADC 通道 Rop（10bit）|
| DS1302 | 实时时钟 + 31B NVRAM | DS1302.h V1.1 | BCD 编码，电池后备 |
| M24C02 | 256B I²C EEPROM | M24C02.h V1.0 | 掉电保存宠物状态用 |
| 振动传感器 | 检测振动 | Vib.h V2.0 | `enumEventVib` |
| 霍尔传感器 | 检测磁铁 | hall.H V2.0 | `enumEventHall` |
| 步进电机 | SM接口 + LED模拟 | StepMotor.h | Phase 4 使用 |
| 红外收发 | 38kHz NEC | IR.h V3.5a | 暂未使用 |
| FM收音机 | 88.7-108MHz | FM_Radio.h V1.1 | 暂未使用 |
| 串口1 | USB/CH340 | uart1.h V2.0 | **保留给 PC 调试** |
| 串口2 | EXT扩展口 | uart2.h V2.0 | **连接 ESP32** |

### ESP32-S3-BOX-3B + BOX-3-DOCK
- **主控 SoC**: ESP32-S3（双核 LX7, WiFi + BLE）
- **模组**: ESP32-S3-WROOM-1
- **开发板**: ESP32-S3-BOX-3B
- **扩展底座**: ESP32-S3-BOX-3-DOCK（提供两组 Pmod 接口，共 16 个 GPIO）
- **开发框架**: Arduino（通过 Arduino IDE）
- **板载屏幕**: 2.4寸 IPS LCD, 320×240, ILI9342C (SPI), 电容触摸
- **板载音频**: 双 MEMS 麦克风 + 喇叭 (ES8311 I²S 编解码)

#### BOX-3-DOCK Pmod GPIO 分布

| 位置 | GPIO 引脚 |
|------|----------|
| 左上 | GPIO4, GPIO38, GPIO19, GPIO21 |
| 右上 | GPIO40, GPIO39, GPIO20, GPIO42 |
| 左下 | GPIO43 (U0TXD), GPIO1, GPIO14, GPIO10 |
| 右下 | GPIO44 (U0RXD), GPIO12, GPIO9, GPIO13 |

#### ESP32 三个 UART 分配

| UART | 引脚 | 用途 | 波特率 | 状态 |
|------|------|------|--------|------|
| UART0 | USB 内置 | PC 调试串口 | 115200 | 已分配 |
| UART2 | **GPIO43**(TX 左下) **GPIO44**(RX 右下) | STC-B 通信 | 9600 | 已分配 |
| **UART1** | **待选 GPIO** (推荐 GPIO40/39 右上) | **ASRPRO 语音** | **9600** | **待实现** |

### ASRPRO 2.0 语音识别模块（已弃用 → 改用 BOX-3B 板载麦克风）
- **原计划**: 外接 ASRPRO 2.0 模块做离线语音识别
- **新方案**: BOX-3B 自带双 MEMS 麦克风 + ES8311 音频编解码，使用 Espressif **ESP-SR** 库做离线语音识别
- **优势**: 省一个硬件模块，减少接线，BOX-3B 板载喇叭可同时播放语音回复
- **备选**: 如果 ESP-SR 识别率不够好，仍可后续接入 ASRPRO（协议文档保留在 `protocol/ASRPRO语音模块接口协议.md`）

### 接线
```
STC-B EXT口                ESP32-S3-BOX-3-DOCK
─────────────              ──────────────────────
TXD (5V) ──电平转换(3.3V)─► GPIO44 右下Pmod (RX2)
RXD        ◄────────────── GPIO43 左下Pmod (TX2) 3.3V
GND        ─────────────── GND

ASRPRO 2.0                ESP32-S3-BOX-3-DOCK（待接线）
──────────                ──────────────────────
TXD        ──────────────► GPIO39 右上Pmod (RX1)
RXD        ◄────────────── GPIO40 右上Pmod (TX1)
GND        ─────────────── GND
VCC        ──── 5V ────── (独立供电或ESP32 5V引脚)
```

> ⚠️ **电平转换（必须）**：ESP32 TX(3.3V) → STC RX 可直连；STC TX(5V) → ESP32 RX(3.3V) **必须**加电平转换（推荐电阻分压：2KΩ+3.3KΩ）。直连会损坏 ESP32-S3。
> ⚠️ **GPIO43/44** 标注 U0TXD/U0RXD 但通过 GPIO Matrix 映射为 UART2，不与 USB 调试冲突。
> ⚠️ **备选引脚**：如 GPIO43/44 实测有冲突，可改用 GPIO40/39（右上）或 GPIO38/4（左上）。
> ⚠️ ASRPRO 供电注意：识别峰值 150mA，如接喇叭建议独立供电。

---

## 三、BSP 框架关键知识

STC-B 的 BSP 是协作式事件驱动框架，新窗口**必须了解**以下要点：

### 核心调用模式
```c
void main() {
    // 1. 各模块初始化
    ModuleInit();
    // 2. 注册事件回调
    SetEventCallBack(enumEventXXX, my_callback);
    // 3. 系统初始化
    MySTC_Init();
    // 4. 主循环
    while(1) { MySTC_OS(); }
}
```

### 必须声明的全局常量
```c
code unsigned long SysClock = 11059200;  // 必须有，否则定时器不准
#ifdef _displayer_H_
code char decode_table[] = { ... };      // 数码管段码表，必须有
#endif
```

### 系统事件（定时器）
| 事件 | 周期 | 本项目用途 |
|------|------|-----------|
| `enumEventSys1mS` | 1ms | （未使用，保留） |
| `enumEventSys10mS` | 10ms | 传感器采样 + 定时上报 |
| `enumEventSys100mS` | 100ms | 表情动画帧更新 |
| `enumEventSys1S` | 1s | （未使用，ESP32侧计时） |

### 外设事件
| 事件 | 本项目用途 |
|------|-----------|
| `enumEventKey` | K1/K2/K3 按键 → 喂食/玩耍/摸头 |
| `enumEventNav` | 导航按键 → 方向交互 |
| `enumEventVib` | 振动 → 拍桌子互动 |
| `enumEventHall` | 霍尔 → 磁铁靠近 = 摸摸 |
| `enumEventUart2Rxd` | 串口接收 → 解析 ESP32 命令 |

### ⚠️ 关键约束
- **所有回调函数执行时间必须 < 1ms**，否则 `PollingMisses` 会增加
- **K3 与 ADC 共用 P1.7**：ADC 启用后 K3 必须用 `GetAdcNavAct(enumAdcNavKey3)` 读取
- **UART2 使用 EXT 口时**：不能调用 `EXTInit()`
- **Music 模块与 Beep/Displayer 互斥**：不能同时使用
- **decode_table** 需要在 main.c 中用 `code` 关键字定义（存在 ROM 中节省 RAM）

---

## 四、通信协议摘要

完整协议见 `protocol/通信协议.md`，此处列出关键要点。

### 帧格式
```
[0xAA] [CMD] [LEN] [DATA x LEN] [XOR校验]
```
- 帧头固定 `0xAA`，最大数据载荷 16 字节，校验 = 全部字段异或

### 核心命令
| 方向 | CMD | 名称 | 数据 |
|------|-----|------|------|
| ESP→STC | 0x01 | SET_EXPRESSION | [表情ID] |
| ESP→STC | 0x03 | SET_LED | [LED掩码] |
| ESP→STC | 0x06 | PLAY_SOUND | [音效ID] |
| ESP→STC | 0x07 | SET_ALL | [表情, LED, 音效] |
| ESP→STC | 0x10 | QUERY_SENSOR | 无 |
| ESP→STC | 0xF0 | SYS_PING | 无 |
| STC→ESP | 0x20 | SENSOR_REPORT | [温度H,L, 光照H,L, 按键, 标志] |
| STC→ESP | 0x22 | EVENT_REPORT | [事件类型, 事件数据] |
| STC→ESP | 0xE0 | ACK_OK | [原CMD] |
| STC→ESP | 0xF0 | SYS_PONG | 无 |

### 通信参数
- 波特率: **9600 bps, 8N1**
- 传感器自动上报: 每 **500ms**
- 心跳间隔: **5s**，连续3次无回应视为离线
- 命令超时重传: **500ms**，最多3次

---

## 五、当前已完成的工作 (Phase 1)

### ✅ STC-B 固件
| 模块 | 状态 | 说明 |
|------|------|------|
| comm.h/c | ✅ 完成 | UART2 帧收发，状态机解析，校验 |
| expression.h/c | ✅ 完成 | 12种表情 + 8种音效 + LED控制 |
| main.c | ✅ 完成 | 命令分发 + 事件回调注册 + 传感器定时上报 |

### ✅ ESP32-S3 固件
| 模块 | 状态 | 说明 |
|------|------|------|
| PetProtocol.h/cpp | ✅ 完成 | 帧解析 + 全部下行发送 + 回调注册 |
| PetState.h/cpp | ✅ 完成 | 8种心情 + 4维属性衰减 + 7种交互 + 成长系统 |
| desktop_pet.ino | ✅ 完成 | setup/loop + 传感器查询 + 心跳 + 状态更新定时 |

### ✅ 文档
| 文档 | 说明 |
|------|------|
| README.md | 项目总览 + 接线图 + 4阶段路线图 |
| QUICKSTART.md | 接线→Keil工程→Arduino工程→联调测试 全流程 |
| 通信协议.md | STC-B ↔ ESP32 完整协议规范（帧格式+命令表+编码表+时序图）|
| ASRPRO语音模块接口协议.md | ASRPRO 2.0 接口协议（已弃用，改由 BOX-3B 板载麦克风+ESP-SR 替代）|
| 屏幕显示架构.md | BOX-3B 屏幕表情系统（像素画+触摸+语音+状态栏+通信协议变更）|

---

## 六、已知问题与 TODO

### 需要联调验证的
1. **STC-B 的 EXT 口引脚确认**：`EXT TXD / RXD / GND` 的物理引脚位置需要对照原理图确认
2. **电平兼容性**：STC-B (5V) → ESP32 (3.3V) 的 GPIO 容忍度需实测
3. **decode_table 扩展**：当前扩展了索引 10-19 的自定义段码，需要实测显示效果并调整
4. **表情段码效果**：`expression.c` 中 `face_table` 是创意段码，实际显示效果需肉眼调试

### 代码中的 TODO
| 文件 | 位置 | 内容 |
|------|------|------|
| STC-B main.c | `CMD_QUERY_TIME` 分支 | 需要集成 DS1302 读取 |
| STC-B main.c | `main()` | `DS1302Init()` 调用被注释，需配置默认时间 |
| STC-B expression.c | `ExprSetCustom()` | 自定义段码直接写入需要 BSP 底层接口支持 |
| STC-B expression.c | `ExprPlaySound()` | 双音/下降音等多段音效需要定时器回调串联 |
| STC-B expression.c | `ExprAnimate()` | 眨眼等动画逻辑预留，待实现 |
| STC-B comm.c | `OnUart2Rxd()` | `rxd_buf[i]` 读取方式需确认 BSP UART2 接收 API 实际行为 |
| ESP32 PetProtocol.cpp | `CMD_TIME_REPORT` | 时间上报解析待实现 |
| ESP32 PetState.cpp | `onButtonPress()` | 喂食/玩耍后自动恢复表情的定时器未实现 |

---

## 七、Phase 2 推进计划

### 优先级排序

**P0 — 联调基础（必须先做）**
1. 硬件接线，分别烧录 STC-B 和 ESP32
2. 验证串口通信：心跳 → 传感器上报 → 命令下发
3. 调试表情显示效果，微调 `decode_table` 和 `face_table`
4. 验证按键/振动/霍尔事件上报

**P1 — 表情动画**
1. 实现眨眼动画（每5秒闭眼100ms再睁开）
2. 实现嘴巴微动（表情细微变化让宠物更"活"）
3. 多段音效串联（定时器回调中播第二段音）

**P2 — EEPROM 存档**
1. 定义存档格式（属性值 + 等级 + 年龄 → M24C02 地址映射）
2. 属性变化时写入（注意 5-10ms 写延时 + 写寿命保护）
3. 上电时读取存档恢复状态
4. ESP32 侧也做一份状态缓存，双保险

**P3 — DS1302 时间感知**
1. 集成 DS1302 读取，实现 `CMD_QUERY_TIME`
2. ESP32 根据时间切换行为（早安/午觉/晚安）
3. 闹钟功能（设定时间触发特殊表情和音效）

**P4 — BOX-3B 屏幕表情系统**（详见 `protocol/屏幕显示架构.md`）
- ✅ 已落地：LovyanGFX 驱动 ILI9342C + `PetState`→`PetDisplay::render()` 联动（矢量画 MVP，8 种心情），文件在 `ESP32/src/desktop_pet/PetDisplay.h/cpp`
- ⬜ 剩余：像素画位图、动画、状态栏、触摸、对话气泡
1. 集成 LovyanGFX 库，驱动 BOX-3B ILI9342C 屏幕 ✅（矢量画版）
2. 设计 2-3 个基础像素画表情（32×32，8× 放大显示）⬜
3. 新增 `PetDisplay.h/cpp`：表情渲染 + 状态栏 + 对话气泡
4. 集成触摸屏：头部=摸头，肚子=挠痒，底部=喂食/玩耍/睡觉按钮
5. PetState → 屏幕表情联动（替代旧的 SET_EXPRESSION 串口命令）
6. 通信协议调整：CMD 0x01 改为 SET_AUX_DISPLAY（数码管辅助信息），新增 CMD 0x08
7. STC-B 端 `expression.c` 简化为数值显示函数

**P5 — BOX-3B 板载语音识别**（替代 ASRPRO 模块）
1. 集成 ESP-SR 库（Espressif 离线语音识别）
2. 配置唤醒词（"小宠物"）+ 命令词
3. 新增 `VoiceModule.h/cpp`：ESP-SR 回调 → PetState
4. 板载喇叭播放语音回复 + 音效
5. STC-B 端零改动

### 后续 Phase 3-4 路线
- **Phase 3**: WiFi + 天气API + NTP + Web管理页
- **Phase 4**: 步进电机动作 + 手机推送 + OTA + 多宠物社交

---

## 八、开发环境备注

### STC-B 端 Keil 工程配置要点
1. **芯片选择**: STC15F2K60S2（或选 generic 8051 后手动配置）
2. **Include Paths**: 添加 BSP 头文件目录 + 本项目 `inc/` 目录
3. **Library**: 添加 BSP 的 `.lib` 文件（如 `BSP_Ver3.6b.lib`）
4. **Output**: 生成 `.hex` 文件用于 STC-ISP 烧录
5. **Memory Model**: Small（RAM 有限，code 关键字将常量存 ROM）

### ESP32-S3 端 Arduino 配置要点
1. **开发板**: ESP32S3 Dev Module
2. **库依赖**: 无第三方库（Phase 1 仅用 Arduino 核心库）
3. **串口**: `Serial` = USB调试(115200)，`Serial2` = STC-B通信(9600)，`Serial1` = ASRPRO语音(9600, 待实现)
4. **后续 Phase 3 可能需要的库**: WiFi.h（内置）、HTTPClient.h（内置）、ArduinoJson（第三方）

---

## 九、关键设计决策记录

| 决策 | 选择 | 原因 |
|------|------|------|
| ESP32 硬件 | BOX-3B + BOX-3-DOCK | 用户实际持有的硬件 |
| STC-B UART2 引脚 | GPIO43(TX) GPIO44(RX) | DOCK 左下/右下 Pmod 可用，通过 GPIO Matrix 映射 UART2 |
| 电平转换 | 电阻分压 2K+3.3K 或电平转换芯片 | STC TX(5V) → ESP32 RX(3.3V) 必须降压 |
| 串口连接 | UART2 (EXT口) | UART1(USB) 保留给 PC 调试和烧录 |
| 屏幕表情风格 | 像素复古风 (32×32 ×8 放大) | 契合 8051 复古调性，开发简单，每帧仅 512 字节 |
| 语音方案 | BOX-3B 板载麦克风 + ESP-SR | 省去 ASRPRO 模块，板载喇叭可同时播放回复 |
| 数码管定位 | 辅助信息显示 | 屏幕做主表情，数码管显示时间/温度/等级等数值 |
| 显示库 | LovyanGFX | 支持 ILI9342C，API 简洁，BOX-3B 社区支持好 |
| 波特率 | 9600 bps | 8051 在 11.0592MHz 下 9600 误差最小 |
| 帧校验 | XOR | 8051 上计算简单，够用 |
| 数据载荷上限 | 16 字节 | 8051 RAM 有限（256B 内部 + 扩展），帧缓冲区不能太大 |
| 表情驱动方式 | Seg7Print 索引 decode_table | 兼容现有 BSP，不侵入底层 |
| 传感器上报 | 500ms 主动推送 | 减少 ESP32 轮询，STC-B 侧定时驱动 |
| 宠物属性衰减 | 分钟级 | 让宠物变化可感知但不至于太快 |
| ESP32 框架 | Arduino | 用户更熟悉，开发效率高 |

---

## 十、待确认问题

### 屏幕表情系统
| # | 问题 | 当前默认值 | 影响 |
|---|------|-----------|------|
| 1 | 像素画风格确认 | 32×32 像素复古风，8×放大 | 决定美术资源和渲染代码 |
| 2 | 触摸区域是否需要视觉按钮 | 底部画 3 个图标按钮 | 影响屏幕布局 |
| 3 | 数码管显示哪些信息 | 时间/温度/等级/饱食度 | ESP32 下发内容的优先级 |

### 语音识别
| # | 问题 | 当前默认值 | 影响 |
|---|------|-----------|------|
| 4 | ESP-SR 识别率是否满足需求 | 先试板载方案 | 如果不行则需回退到 ASRPRO |
| 5 | 唤醒词选择 | "小宠物" | 在 ESP-SR 中配置 |
| 6 | 是否需要语音合成(TTS)回复 | 先做预录音效 | TTS 需要更多 Flash 和算力 |

> 屏幕架构文档：`protocol/屏幕显示架构.md`
> ASRPRO 备选文档：`protocol/ASRPRO语音模块接口协议.md`（如 ESP-SR 不行可回退）

---

## 十一、关键设计决策记录（补充）

| 决策 | 选择 | 原因 |
|------|------|------|
| ASRPRO 接入点 | ESP32 UART1 (推荐 GPIO40/39 右上Pmod) | UART2 已占用 GPIO43/44(STC-B)，UART1 用其他 Pmod GPIO |
| 语音命令复用 | 映射到已有交互函数 | 语音只是新输入源，STC-B 端零改动，减少复杂度 |
| ASRPRO 帧格式 | `AA 55 ID 55 AA` (5字节固定) | ASRPRO 硬件内置协议，无法修改，只需解析 |
| 语音回复 | ASRPRO 板载处理 | 不经过 ESP32/STC-B，降低延迟，ESP32 只管命令ID |

---

*本文档由 AI 自动生成，用于项目交接。如需更新，请在项目推进后重新生成交付文档。*
