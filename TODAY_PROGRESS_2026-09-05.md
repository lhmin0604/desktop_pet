# 🐾 STC-B 模块进展总结 — 2026-09-05

> **日期**: 2026-09-05  
> **模块**: STC-B 主控固件 (STC15F2K60S2)  
> **当前状态**: Phase 1 代码审查 + Bug 修复 + 死代码清理  
> **修改文件**: `STC-B/source/expression.c`, `STC-B/source/main.c`

---

## 一、今日已完成工作

### ✅ Bug 修复

| 问题 | 文件 | 修复内容 | 状态 |
|------|------|----------|------|
| 音效覆盖 Bug (问题6) | `expression.c` | 修复 `SOUND_HAPPY` / `SOUND_SAD` 被后续 `SetBeep()` 无条件覆盖的问题 | ✅ 已修复 |

**修复详情**:

原代码在 `ExprPlaySound()` 中，先调用 `SetBeep(freq, time)` 从 `sound_table` 读取参数，然后对 `SOUND_HAPPY` / `SOUND_SAD` 又无条件调用 `SetBeep(800, 10)` / `SetBeep(800, 15)`，导致 sound_table 的值永远不会被播放。

修复后：
- 特殊音效（HAPPY/SAD）提前处理并 `return`，不再走 sound_table 通用路径
- 三个分支（HAPPY / SAD / 普通音效）统一先检查 `GetBeepStatus() != enumBeepFree` 再调用 `SetBeep`
- 音效 ID 和协议不变，未修改 `ESP32/`

### ✅ 代码审查与清理

| 问题 | 文件 | 修复内容 | 状态 |
|------|------|----------|------|
| 死代码清理 | `main.c` | 删除 `sensor_dirty` 声明及 5 处赋值（被写入但从未被读取） | ✅ 已清理 |
| 未使用变量 | `main.c` | 删除 `OnNavEvent()` 中未使用的 `unsigned char dir` | ✅ 已清理 |
| 未使用变量 | `main.c` | 删除 `CMD_SYS_PING` 中未使用的 `unsigned char pong` | ✅ 已清理 |
| 注释错误 | `main.c` | 修正 `AdcInit(ADCexpEXT)` 注释（原注释"不使用EXT的ADC"与代码矛盾） | ✅ 已修正 |

### ✅ 问题排查与分析

| 问题 | 分析结果 | 结论 |
|------|----------|------|
| `sensor_flags` 并发访问 (问题4) | BSP V3.6 是协作式事件驱动框架，所有回调在主循环中顺序执行，不存在并发写入可能 | ❌ 非 Bug，已回退修改 |

**分析详情**:

原始代码采用"先发送后清除"模式：
```c
CommSendSensorReport(..., sensor_flags);  // UART TX ~10ms @ 9600bps
sensor_flags = 0;                          // 清除期间新到的事件也被抹掉
```

经分析确认：
- `OnVibEvent`、`OnHallEvent`、`On10msTick`、`OnCommand` 全部通过 `SetEventCallBack()` 注册
- 它们由主循环中的 `MySTC_OS()` 调度执行，**不是 ISR 直接调用**
- 项目源码中没有任何 `interrupt` 关键字或 ISR 函数
- 所有回调在主循环上下文中**顺序执行、不会互相抢占**

结论：在协作式框架下，`sensor_flags` 不存在并发写入的可能，原始代码不会丢失事件。

### ✅ 输入模块功能审查

对 STC-B 的所有输入模块进行了逐项审查，确认已按协议文档实现：

| 输入模块 | 函数 | 协议对照 | 状态 |
|----------|------|----------|------|
| 按键 (K1/K2/K3) | `OnKeyEvent()` | `EVENT_KEY(0x01/0x02/0x03)` + `sensor_buttons` 按位跟踪 | ✅ 正确 |
| 导航 (5方向) | `OnNavEvent()` | `EVENT_NAV(上=5,下=2,左=4,右=1,中=3)` | ✅ 正确 |
| 振动 | `OnVibEvent()` | `sensor_flags bit0` + `EVENT_VIB(0x01)` | ✅ 正确 |
| 霍尔 | `OnHallEvent()` | `sensor_flags bit1` + `EVENT_HALL(0x01=靠近, 0x00=离开)` | ✅ 正确 |
| 事件上报 | `CommSendEvent()` | 硬件事件立即通过 UART2 发送 `CMD_EVENT_REPORT` | ✅ 正确 |
| 周期上报 | `On10msTick()` | 每 500ms 发送 `CMD_SENSOR_REPORT` | ✅ 正确 |

---

## 二、当前代码状态

### 修改文件清单

```
 M STC-B/source/expression.c   (19 insertions, 9 deletions)
 M STC-B/source/main.c         (16 insertions, 23 deletions)
```

### 未修改的文件

- `STC-B/source/comm.c` — 通信模块，无需修改
- `STC-B/inc/comm.h` — 协议定义，无需修改
- `STC-B/inc/expression.h` — 接口声明，无需修改
- `ESP32/` — 严禁修改，未触碰

---

## 三、待完成事项

### 需要 BSP 源码确认的问题

| 问题 | 需要的 BSP 信息 | 用途 |
|------|----------------|------|
| K3 与 ADC 导航引脚冲突 | `ADCexpEXT` 模式下 P1.7 是否被 ADC 占用 | 确认 K3 读取方式是否正确 |
| `decode_table` 归属 | `Seg7Print` 是使用外部 `decode_table` 还是内部表 | 确认 main.c 的 `decode_table` 是否需要保留 |
| 蜂鸣器完成回调 | BSP 是否提供蜂鸣器完成回调，或需要用 `On10msTick` 做状态机 | 实现双音音效第二拍 |

### 功能缺失（需 BSP API）

| 功能 | 状态 | 阻塞原因 |
|------|------|----------|
| `CMD_QUERY_TIME` 时间查询 | ❌ 未实现 | 需要 `DS1302.h` 的 API（读取时间的函数签名、`struct_RTC` 定义） |
| `DS1302Init()` 初始化 | ❌ 被注释 | 同上 |
| 双音音效第二拍 | ❌ 未实现 | 需要 BSP 蜂鸣器完成回调或非阻塞延时 API |
| `ExprSetCustom()` 自定义段码 | ❌ 未实现 | 需要 BSP 暴露数码管扫描缓冲区底层接口 |

### 需要上板验证的项目

| 验证项 | 测试方法 |
|--------|----------|
| `AdcInit(ADCexpEXT)` 是否与 UART2 冲突 | 硬件联调时测试 ADC 读取和串口通信是否同时正常 |
| 传感器定时上报间隔 500ms 是否合适 | 串口抓包查看上报间隔 |
| 音效修复后 SOUND_HAPPY/SOUND_SAD 是否正常播放 | 发送 `CMD_PLAY_SOUND(0x02)` 和 `CMD_PLAY_SOUND(0x03)` 测试 |
| 按键/振动/霍尔事件是否立即上报 | 触发硬件事件，观察 ESP32 串口是否收到 `CMD_EVENT_REPORT` |

---

## 四、编译准备

### 当前状态

- ❌ 项目没有 `.uvproj` / `.uvprojx` 工程文件
- ❌ 没有 Makefile 或构建脚本
- ❌ 无法在当前环境直接编译（缺少 BSP 库文件）

### 需要手动创建 Keil 工程

1. 打开 Keil uVision → Project → New Project
2. 选择芯片：`STC15F2K60S2`
3. 添加源文件：
   ```
   source/main.c
   source/comm.c
   source/expression.c
   ```
4. 添加头文件路径：`inc/` 目录
5. 添加 BSP 库：
   - BSP 的 `.lib` 文件（如 `BSP_Ver3.6b.lib`）
   - BSP 的头文件（`sys.H`, `displayer.H`, `Beep.h`, `Key.h`, `adc.h`, `DS1302.h`, `Vib.h`, `hall.H`, `uart2.h`）
6. 编译：Build → Build Target (F7)

### 潜在编译问题

- **未定义符号**：所有 BSP 函数依赖 BSP 头文件和库，当前项目未包含
- **RAM/ROM 风险**：
  - `face_table[12][8]` = 96 字节 ROM（未使用，可删除）
  - `sound_table[8][2]` = 16 字节 ROM
  - `decode_table[20]` = 20 字节 ROM（可能未使用）
  - 总 ROM 占用约 132 字节，STC15F2K60S2 有 60KB Flash，无风险
  - RAM 占用：传感器缓存 + 通信缓冲区约 100 字节，STC15F2K60S2 有 1KB RAM，无风险

---

## 五、问题分类总结

### A. 必须修复（已完成）

- ✅ 问题6：音效覆盖 Bug — 已修复

### B. 建议修复（已完成）

- ✅ 死代码清理：`sensor_dirty`、未使用变量、注释错误 — 已清理

### C. 暂不处理

- 问题4：`sensor_flags` 并发访问 — 经分析确认非 Bug，已回退修改
- 问题10：`CMD_SYS_PING` 和 `CMD_SYS_PONG` 使用相同命令码 0xF0 — 协议设计，非 Bug

### D. 必须通过硬件/联调验证

- `AdcInit(ADCexpEXT)` 是否与 UART2 冲突
- 传感器定时上报间隔是否合适
- 音效修复后是否正常播放
- 硬件事件是否立即上报

---

## 六、参考文档

本次工作参考了以下队友文档的结构和格式：

- **`HANDOFF.md`** — 项目交付文档，包含"当前已完成的工作"表格、"已知问题与 TODO"列表、"需要联调验证的"项目
- **`protocol/屏幕显示架构.md`** — ESP32 屏幕模块的详细进展文档，包含状态标记（✅/❌）、设计目标表格、实现细节

---

**报告人**: AI Agent  
**生成时间**: 2026-09-05  
**下次更新**: 待 BSP 源码提供后继续推进
