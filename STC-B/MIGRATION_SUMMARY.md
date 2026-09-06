# STC-B SDCC 迁移总结

## 1. 队友 SDCC 工程位置

**分支**: `main` (commit `c320883`)
**文件**:
- `STC-B/inc/stc15_sdcc.h` - STC15F2K60S2 SFR 定义 (SDCC 兼容)
- `STC-B/inc/struct_ADC.h` - ADC 结构体定义
- `STC-B/source/crt0_sdcc.asm` - 启动代码
- `STC-B/source/bsp485_sdcc.c/h` - UART2 驱动 (已修改)
- `STC-B/source/comm_pawbox_sdcc.c` - 协议帧解析 (已重写)
- `STC-B/source/main_pawbox_sdcc.c` - 主程序 (已重写)
- `STC-B/build_sdcc.bat` - Windows 编译脚本

## 2. 复用的 BSP 组件

| 文件 | 用途 | 修改情况 |
|------|------|----------|
| `stc15_sdcc.h` | SFR 定义 (__sfr/__sbit) | 直接复用 |
| `struct_ADC.h` | ADC 数据结构 | 直接复用 |
| `crt0_sdcc.asm` | 启动代码 | 直接复用 |
| `bsp485_sdcc.c` | UART2 驱动 | **修改**: 改为 P1.0/P1.1 (EXT口), 去掉 RS485 DE 控制 |

## 3. 修改/新增的 STC-B 文件

### 新增外设驱动 (全部用 SDCC 直接操作寄存器):

| 文件 | 功能 | 引脚 |
|------|------|------|
| `sys.c/h` | 系统定时器 (1ms 时基) | Timer0 |
| `display.c/h` | 数码管动态扫描 + LED | P0(段码), P2.0-2(位选), P2.3(LED) |
| `beep.c/h` | 蜂鸣器 (Timer1 翻转) | P3.4 |
| `keys.c/h` | K1/K2/K3 按键 (10ms 消抖) | P3.2, P3.3, P1.7 |
| `adc_drv.c/h` | ADC 采样 (导航/温度/光照) | P1.2(ADC2), P1.6(ADC6), P1.7(ADC7) |
| `vib.c/h` | 振动电机 + 传感器 | P2.4(输出), P3.5(输入) |
| `hall.c/h` | 霍尔传感器 | P2.5 |

### 重写的应用层:

| 文件 | 说明 |
|------|------|
| `protocol_sdcc.h` | 协议定义 (匹配 ESP32 PetProtocol) |
| `comm_pawbox_sdcc.c` | 帧解析/发送 (XOR 校验, 非 CRC8) |
| `expression.c/h` | 表情引擎 (12 种预设表情 + 音效) |
| `main_pawbox_sdcc.c` | 主程序 (整合所有功能) |

### 构建系统:

| 文件 | 说明 |
|------|------|
| `Makefile` | Linux SDCC 编译脚本 |
| `build_sdcc.bat` | Windows SDCC 编译脚本 (队友原有) |

## 4. 编译状态

**未编译** - SDCC 未安装且网络受限无法下载

### 编译步骤 (需要 SDCC 4.x):

**Linux**:
```bash
cd STC-B
make
# 输出: output/DesktopPet_STC.ihx
```

**Windows**:
```cmd
cd STC-B
build_sdcc.bat
# 输出: output\DesktopPet_STC.ihx
```

**安装 SDCC**:
- Linux: `sudo apt install sdcc` 或从 https://sdcc.sourceforge.net 下载
- Windows: 从 https://sdcc.sourceforge.net 下载安装, 设置 `SDCC_HOME` 环境变量

## 5. HEX 文件位置

编译成功后生成:
- `STC-B/output/DesktopPet_STC.ihx` (Intel HEX, stc-isp 可直接烧录)
- `STC-B/output/DesktopPet_STC.bin` (二进制, 可选)

## 6. 需要上板验证的功能

### 关键引脚 (需确认原理图):

| 功能 | 假设引脚 | 验证方法 |
|------|----------|----------|
| 振动传感器输入 | P3.5 | 触发振动, 观察串口事件上报 |
| 霍尔传感器 | P2.5 | 磁铁靠近/远离, 观察事件上报 |
| 五向导航 ADC 阈值 | P1.2/ADC2 | 按下各方向, 记录 ADC 值, 调整 `adc_drv.c` 阈值 |

### 功能验证清单:

- [ ] **UART2 通信**: ESP32 发送 PING (0xAA 0xF0 0x00 0x0F), STC-B 应答 PONG
- [ ] **按键 K1/K2/K3**: 按下时串口收到 EVENT_KEY 事件
- [ ] **五向导航**: 按下上/下/左/右/中, 串口收到 EVENT_NAV 事件
- [ ] **表情显示**: 发送 CMD_SET_EXPRESSION, 数码管显示对应表情
- [ ] **蜂鸣器**: 发送 CMD_SET_BUZZER, 听到蜂鸣声
- [ ] **LED**: 发送 CMD_SET_LED, LED 亮灭
- [ ] **温度/光照**: 发送 CMD_QUERY_SENSOR, 收到 ADC 值
- [ ] **振动传感器**: 触发振动, 收到 EVENT_VIB 事件
- [ ] **霍尔传感器**: 磁铁靠近/远离, 收到 EVENT_HALL 事件
- [ ] **传感器定时上报**: 每 500ms 自动上报一次传感器数据

### 可能需要调整的参数:

1. **五向导航 ADC 阈值** (`adc_drv.c`):
   ```c
   #define NAV_TH_CENTER   30
   #define NAV_TH_RIGHT    130
   #define NAV_TH_UP       280
   #define NAV_TH_DOWN     480
   #define NAV_TH_LEFT     680
   ```
   上板后用串口打印 ADC 原始值, 根据实际电阻分压调整

2. **振动/霍尔引脚** (`vib.c`, `hall.c`):
   如果 P3.5/P2.5 不对, 修改对应文件中的引脚定义

## 7. 协议格式

**帧格式** (与 ESP32 PetProtocol 一致):
```
[0xAA] [CMD] [LEN] [DATA[0..LEN-1]] [CHK]
CHK = HEAD ^ CMD ^ LEN ^ DATA[0..LEN-1]
```

**命令集** (见 `protocol_sdcc.h`):
- 下行: SET_EXPRESSION(0x01), SET_LED(0x03), SET_BUZZER(0x04), PING(0xF0), ...
- 上行: SENSOR_REPORT(0x20), EVENT_REPORT(0x22), ACK_OK(0xE0), PONG(0xF0), ...

## 8. 未迁移的功能

- **DS1302 RTC**: 原代码有 TODO, 未实现 (需要 P5.4/P4.0/P4.2 引脚驱动)
- **步进电机**: 原代码标注 Phase 4, 未实现
- **I2C EEPROM**: 与 DS1302 共用 P4.0, 硬件冲突, 未实现

## 9. 文件清单

```
STC-B/
├── Makefile                    # Linux 编译脚本
├── build_sdcc.bat              # Windows 编译脚本
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
└── output/                     # 编译产物 (make 后生成)
    └── DesktopPet_STC.ihx
```

## 10. 下一步

1. **安装 SDCC 4.x**: https://sdcc.sourceforge.net
2. **编译**: `cd STC-B && make`
3. **烧录**: 用 stc-isp 打开 `output/DesktopPet_STC.ihx` 烧录
4. **验证**: 按"功能验证清单"逐项测试
5. **调参**: 根据实际硬件调整 ADC 阈值和引脚定义
