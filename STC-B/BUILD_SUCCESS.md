# STC-B SDCC 迁移完成报告

## ✅ 编译状态: 成功

**编译时间**: 2026-09-06  
**SDCC 版本**: 3.8.0  
**目标芯片**: STC15F2K60S2 / IAP15F2K61S2  
**编译选项**: `--model-large --opt-code-size`

---

## 1. 队友 SDCC 工程位置

**分支**: `main` (commit `c320883`)  
**位置**: `STC-B/` 目录下的 SDCC 相关文件

**复用的核心文件**:
- `inc/stc15_sdcc.h` - STC15F2K60S2 SFR 定义 (直接复用)
- `inc/struct_ADC.h` - ADC 数据结构 (直接复用)
- `source/crt0_sdcc.asm` - 启动代码 (直接复用)
- `source/bsp485_sdcc.c/h` - UART2 驱动 (已修改)
- `build_sdcc.bat` - Windows 编译脚本 (保留)

---

## 2. 复用的 BSP 组件

| 组件 | 原文件 | 修改情况 | 说明 |
|------|--------|----------|------|
| SFR 定义 | `stc15_sdcc.h` | 直接复用 | 所有 __sfr/__sbit 定义 |
| 启动代码 | `crt0_sdcc.asm` | 直接复用 | 8051 启动 + 中断向量 |
| UART2 驱动 | `bsp485_sdcc.c` | **已修改** | 改为 P1.0/P1.1 (EXT口), 去掉 RS485 DE 控制 |

**关键修改**:
- UART2 从 P3.6/P3.7 (RS485) 改为 P1.0/P1.1 (EXT口, 接 ESP32)
- 去掉了 RS485 方向控制引脚 (DE)
- 保留 9600bps 波特率, 使用独立波特率发生器 BRT

---

## 3. 修改/新增的 STC-B 文件

### 新增外设驱动 (11 个文件)

| 文件 | 功能 | 引脚分配 | 实现方式 |
|------|------|----------|----------|
| `sys.c/h` | 系统定时器 (1ms 时基) | Timer0 | 16-bit auto-reload, 1T 模式 |
| `display.c/h` | 数码管动态扫描 + LED | P0(段码), P2.0-2(位选), P2.3(LED) | 74HC138 译码, 2ms 刷新 |
| `beep.c/h` | 蜂鸣器 (Timer1 翻转) | P3.4 | 16-bit auto-reload, 频率可调 |
| `keys.c/h` | K1/K2/K3 按键 | P3.2, P3.3, P1.7 | 10ms 消抖, press/release 检测 |
| `adc_drv.c/h` | ADC 采样 | P1.2(ADC2), P1.6(ADC6), P1.7(ADC7) | 10-bit, 轮流采样 3 通道 |
| `vib.c/h` | 振动电机 + 传感器 | P2.4(输出), P3.5(输入) | GPIO 控制 |
| `hall.c/h` | 霍尔传感器 | P2.5 | GPIO 输入, active LOW |

### 重写的应用层 (4 个文件)

| 文件 | 功能 | 说明 |
|------|------|------|
| `protocol_sdcc.h` | 协议定义 | 匹配 ESP32 PetProtocol, XOR 校验 |
| `comm_pawbox_sdcc.c` | 帧解析/发送 | 状态机解析, 支持所有命令 |
| `expression.c/h` | 表情引擎 | 12 种预设表情 + 8 种音效 |
| `main_pawbox_sdcc.c` | 主程序 | 整合所有功能, 事件驱动 |

### 构建系统 (2 个文件)

| 文件 | 说明 |
|------|------|
| `Makefile` | Linux SDCC 编译脚本 (新增) |
| `build_sdcc.bat` | Windows SDCC 编译脚本 (队友原有, 保留) |

---

## 4. SDCC 编译结果

**✅ 编译成功**

```
所有 11 个 C 文件编译通过
启动代码汇编成功
链接成功, 生成 HEX 文件
```

**编译警告** (1 个, 可忽略):
- `main_pawbox_sdcc.c:303: warning 126: unreachable code` 
  - 原因: `return 0;` 在 `while(1)` 之后, 永远不会执行
  - 影响: 无

---

## 5. HEX 文件位置

**输出目录**: `STC-B/output/`

**关键文件**:
- `DesktopPet_STC.ihx` (12 KB) - **Intel HEX, stc-isp 可直接烧录**
- `DesktopPet_STC.map` (30 KB) - 内存映射表
- `DesktopPet_STC.mem` (1.2 KB) - 内存使用摘要

**内存使用**:
- ROM/FLASH: ~95 bytes (代码段)
- External RAM: 165 bytes (全局变量)
- Internal RAM: 栈空间 230 bytes

**烧录方法**:
1. 打开 stc-isp 软件
2. 选择芯片: STC15F2K60S2 或 IAP15F2K61S2
3. 加载文件: `STC-B/output/DesktopPet_STC.ihx`
4. 选择串口号, 点击下载
5. 给 STC-B 板重新上电

---

## 6. 需要上板验证的功能

### 关键引脚假设 (需确认原理图)

| 功能 | 假设引脚 | 验证方法 | 优先级 |
|------|----------|----------|--------|
| 振动传感器输入 | P3.5 | 触发振动, 观察串口事件上报 | 高 |
| 霍尔传感器 | P2.5 | 磁铁靠近/远离, 观察事件上报 | 高 |
| 五向导航 ADC 阈值 | P1.2/ADC2 | 按下各方向, 记录 ADC 值 | 中 |

### 功能验证清单

#### 基础通信
- [ ] **UART2 通信**: ESP32 发送 PING (0xAA 0xF0 0x00 0x0F), STC-B 应答 PONG
- [ ] **波特率**: 确认 9600bps 通信正常

#### 输入设备
- [ ] **按键 K1**: 按下时串口收到 `EVENT_KEY 0x01`
- [ ] **按键 K2**: 按下时串口收到 `EVENT_KEY 0x02`
- [ ] **按键 K3**: 按下时串口收到 `EVENT_KEY 0x03`
- [ ] **五向导航-上**: 收到 `EVENT_NAV 0x05`
- [ ] **五向导航-下**: 收到 `EVENT_NAV 0x02`
- [ ] **五向导航-左**: 收到 `EVENT_NAV 0x04`
- [ ] **五向导航-右**: 收到 `EVENT_NAV 0x01`
- [ ] **五向导航-中**: 收到 `EVENT_NAV 0x03`
- [ ] **振动传感器**: 触发振动, 收到 `EVENT_VIB 0x01`
- [ ] **霍尔传感器**: 磁铁靠近, 收到 `EVENT_HALL 0x01`; 远离, 收到 `EVENT_HALL 0x00`

#### 输出设备
- [ ] **表情显示**: 发送 `CMD_SET_EXPRESSION 0x00` (笑脸), 数码管显示 ◠‿◠
- [ ] **表情切换**: 发送不同表情 ID (0x00-0x0B), 验证 12 种表情
- [ ] **自定义段码**: 发送 `CMD_SET_CUSTOM_FACE`, 验证 8 字节段码
- [ ] **蜂鸣器**: 发送 `CMD_SET_BUZZER [freq_H, freq_L, time_H, time_L]`, 听到蜂鸣声
- [ ] **LED**: 发送 `CMD_SET_LED 0x01`, LED 亮; 发送 0x00, LED 灭
- [ ] **音效播放**: 发送 `CMD_PLAY_SOUND 0x01-0x08`, 验证 8 种音效

#### 传感器
- [ ] **温度 ADC**: 发送 `CMD_QUERY_SENSOR`, 收到 ADC 值 (ADC6)
- [ ] **光照 ADC**: 发送 `CMD_QUERY_SENSOR`, 收到 ADC 值 (ADC7)
- [ ] **定时上报**: 每 500ms 自动上报一次传感器数据

#### 系统
- [ ] **开机表情**: 上电后数码管显示笑脸, LED0 亮, 蜂鸣器响 100ms
- [ ] **心跳**: 发送 PING, 收到 PONG
- [ ] **重置**: 发送 RESET, 表情恢复笑脸, LED 灭

### 可能需要调整的参数

#### 1. 五向导航 ADC 阈值 (`adc_drv.c`)

当前阈值 (需上板校准):
```c
#define NAV_TH_CENTER   30    // 0-30: 中心按下
#define NAV_TH_RIGHT    130   // 31-130: 右
#define NAV_TH_UP       280   // 131-280: 上
#define NAV_TH_DOWN     480   // 281-480: 下
#define NAV_TH_LEFT     680   // 481-680: 左
// >680: 释放
```

**校准方法**:
1. 在 `adc_poll()` 中添加串口打印: `printf("ADC2=%d\n", adc_val_nav);`
2. 按下每个方向, 记录 ADC 值
3. 根据实际值调整阈值

#### 2. 振动/霍尔引脚 (`vib.c`, `hall.c`)

如果 P3.5/P2.5 不对, 修改对应文件中的引脚定义:
```c
// vib.c
#define VIB_SENSOR_PIN  P35  // 改为实际引脚

// hall.c
#define HALL_PIN        P25  // 改为实际引脚
```

---

## 7. 协议格式

### 帧结构
```
[0xAA] [CMD] [LEN] [DATA[0..LEN-1]] [CHK]
CHK = HEAD ^ CMD ^ LEN ^ DATA[0..LEN-1]
```

### 命令集 (完整列表)

**下行命令 (ESP32 → STC-B)**:
| CMD | 名称 | 数据格式 | 说明 |
|-----|------|----------|------|
| 0x01 | SET_EXPRESSION | [expr_id] | 设置表情 (0x00-0x0B) |
| 0x02 | SET_CUSTOM_FACE | [s0..s7] | 自定义 8 字节段码 |
| 0x03 | SET_LED | [led_byte] | 设置 LED (bit0-7) |
| 0x04 | SET_BUZZER | [freq_H, freq_L, time_H, time_L] | 蜂鸣器 (Hz, 10ms) |
| 0x05 | SET_MOTOR | [speed, steps_H, steps_L] | 步进电机 (TODO) |
| 0x06 | PLAY_SOUND | [sound_id] | 播放音效 (0x01-0x08) |
| 0x07 | SET_ALL | [expr, led, sound] | 一次设置全部 |
| 0x10 | QUERY_SENSOR | - | 查询传感器 |
| 0x11 | QUERY_TIME | - | 查询时间 (TODO) |
| 0xF0 | SYS_PING | - | 心跳 |
| 0xF1 | SYS_RESET | - | 重置 |

**上行命令 (STC-B → ESP32)**:
| CMD | 名称 | 数据格式 | 说明 |
|-----|------|----------|------|
| 0x20 | SENSOR_REPORT | [temp_H, temp_L, light_H, light_L, buttons, flags] | 传感器上报 |
| 0x21 | TIME_REPORT | [year, month, day, weekday, hour, minute, second] | 时间上报 (TODO) |
| 0x22 | EVENT_REPORT | [event_type, event_data] | 事件上报 |
| 0xE0 | ACK_OK | [orig_cmd] | 成功确认 |
| 0xE1 | ACK_FAIL | [orig_cmd, error] | 失败确认 |
| 0xF0 | SYS_PONG | - | 心跳回应 |

**事件类型**:
- 0x01: EVENT_KEY (按键)
- 0x02: EVENT_NAV (导航键)
- 0x03: EVENT_VIB (振动)
- 0x04: EVENT_HALL (霍尔)

---

## 8. 未迁移的功能

| 功能 | 原因 | 后续计划 |
|------|------|----------|
| DS1302 RTC | 需要 P5.4/P4.0/P4.2 引脚驱动, 原代码有 TODO | Phase 3 |
| 步进电机 | 原代码标注 Phase 4 | Phase 4 |
| I2C EEPROM | 与 DS1302 共用 P4.0, 硬件冲突 | 不使用 |

---

## 9. 文件清单

```
STC-B/
├── Makefile                    # Linux 编译脚本
├── build_sdcc.bat              # Windows 编译脚本
├── MIGRATION_SUMMARY.md        # 迁移总结
├── BUILD_SUCCESS.md            # 本报告
├── inc/
│   ├── stc15_sdcc.h           # SFR 定义 (队友)
│   ├── struct_ADC.h           # ADC 结构 (队友)
│   ├── protocol_sdcc.h        # 协议定义
│   ├── sys.h                  # 系统定时器
│   ├── display.h              # 数码管驱动
│   ├── beep.h                 # 蜂鸣器驱动
│   ├── keys.h                 # 按键驱动
│   ├── adc_drv.h              # ADC 驱动
│   ├── vib.h                  # 振动驱动
│   ├── hall.h                 # 霍尔驱动
│   └── expression.h           # 表情引擎
├── source/
│   ├── crt0_sdcc.asm          # 启动代码 (队友)
│   ├── bsp485_sdcc.c/h        # UART2 驱动 (修改自队友)
│   ├── comm_pawbox_sdcc.c     # 协议帧解析
│   ├── sys.c                  # 系统定时器
│   ├── display.c              # 数码管驱动
│   ├── beep.c                 # 蜂鸣器驱动
│   ├── keys.c                 # 按键驱动
│   ├── adc_drv.c              # ADC 驱动
│   ├── vib.c                  # 振动驱动
│   ├── hall.c                 # 霍尔驱动
│   ├── expression.c           # 表情引擎
│   └── main_pawbox_sdcc.c     # 主程序
└── output/                     # 编译产物
    ├── DesktopPet_STC.ihx     # Intel HEX (12 KB)
    ├── DesktopPet_STC.map     # 内存映射 (30 KB)
    └── DesktopPet_STC.mem     # 内存摘要 (1.2 KB)
```

---

## 10. 下一步操作

### 立即执行
1. **烧录固件**: 用 stc-isp 烧录 `output/DesktopPet_STC.ihx`
2. **基础测试**: 验证 UART2 通信 (PING/PONG)
3. **按键测试**: 验证 K1/K2/K3 和五向导航

### 调试步骤
1. **串口调试**: 使用串口助手查看 STC-B 上报的数据
2. **ADC 校准**: 记录五向导航各方向的 ADC 值, 调整阈值
3. **引脚确认**: 如果振动/霍尔不工作, 确认实际引脚并修改代码

### 功能扩展
1. **DS1302 RTC**: 实现时间读取/设置 (Phase 3)
2. **步进电机**: 实现电机控制 (Phase 4)
3. **表情动画**: 实现眨眼、嘴巴动作等 (在 `expr_animate()`)

---

## 11. 技术要点

### SDCC 与 C51 的差异
- `__sfr __at(addr)` 替代 `sfr name = addr`
- `__sbit __at(addr)` 替代 `sbit name = addr^bit`
- `__interrupt(N)` 替代 `interrupt N`
- `__using(N)` 替代 `using N`
- 不支持结构体赋值: `struct a = b;` ❌ → 需逐字段赋值 ✅
- 2D 数组在 code 区有兼容性问题, 改用 1D 数组 ✅

### 内存模型
- 使用 `--model-large`: 默认指针为 24-bit, 可访问全部 64KB XRAM
- 代码放在 ROM, 变量放在 XRAM
- 栈在内部 RAM (0x1A-0xFF, 230 bytes)

### 中断优先级
- Timer0 (1ms 时基): 最高优先级
- UART2 (通信): 高优先级
- Timer1 (蜂鸣器): 普通优先级

---

## 12. 联系与反馈

如果上板验证时遇到问题:
1. 检查电源电压 (5V)
2. 确认晶振频率 (11.0592MHz)
3. 验证串口连接 (P1.0/P1.1)
4. 查看串口输出是否有数据

**编译环境**:
- SDCC 3.8.0 (Ubuntu 20.04)
- 如需更高版本, 建议 SDCC 4.x

---

**迁移完成时间**: 2026-09-06  
**总代码行数**: ~2500 行 (含注释)  
**编译产物大小**: 12 KB (HEX)
