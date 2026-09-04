# 🚀 Phase 1 快速开始指南

## 第一步：硬件连接 (5分钟)

### 你需要的东西
- STC-B 学习板 ×1（已烧录好程序）
- ESP32-S3-BOX-3B + BOX-3-DOCK ×1
- 杜邦线 ×3（公对母或母对母，看你的接口）
- 电平转换：2KΩ+3.3KΩ 电阻 或 双向逻辑电平转换模块（STC 5V TX → ESP32 3.3V RX）

### 接线

```
STC-B EXT口              ESP32-S3-BOX-3-DOCK
─────────────            ──────────────────────
TXD (5V) ──电平转换───► GPIO44 右下Pmod (RX2)
RXD      ◄────────────── GPIO43 左下Pmod (TX2)
GND      ────────────── GND
```

**电平转换接法（电阻分压方案）**:
```
STC EXT_TXD (5V) ──── 2KΩ ──┬──── GPIO44 (ESP32 RX)
                              │
                             3.3KΩ
                              │
                             GND
```
分压比 = 3.3K/(2K+3.3K) ≈ 0.62，5V × 0.62 ≈ 3.1V ✅

**ESP32 GPIO43** 在 DOCK **左下** Pmod 第1针（标注 U0TXD）
**ESP32 GPIO44** 在 DOCK **右下** Pmod 第1针（标注 U0RXD）

---

## 第二步：烧录 STC-B (15分钟)

### 在 Keil 中创建工程

1. 打开 Keil uVision → Project → New Project
2. 选择芯片：在列表中找到 `STC15F2K60S2`（或在 `Legacy Device Database` 中选类似的 8051 芯片）
3. 添加源文件：
   ```
   source/main.c
   source/comm.c
   source/expression.c
   ```
4. 添加头文件路径：将 `inc/` 目录加入 Include Paths
5. 添加 BSP 库：
   - 将 BSP 的 `.lib` 文件（如 `BSP_Ver3.6b.lib`）加入工程
   - 将 BSP 的头文件（`sys.H`, `displayer.H` 等）加入 Include Paths
6. 编译：Build → Build Target (F7)
   - 如果有编译错误，检查头文件路径是否正确

### 烧录到板子

1. 打开 STC-ISP
2. 选择芯片型号 `STC15F2K60S2`
3. 选择串口号（USB 连接后出现的 COM 口）
4. 加载编译生成的 `.hex` 文件
5. 点击"下载/编程"
6. 按板子上的复位键开始烧录

---

## 第三步：烧录 ESP32-S3 (10分钟)

### 安装 Arduino 环境

1. 下载 [Arduino IDE](https://www.arduino.cc/en/software)
2. 添加 ESP32 开发板支持：
   - File → Preferences → Additional Boards Manager URLs:
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Tools → Board → Boards Manager → 搜索 "esp32" → Install

### 打开工程

1. 打开 `ESP32/src/desktop_pet/desktop_pet.ino`（已配置为 BOX-3-DOCK 引脚 GPIO43/44）
2. 同目录下已有 `PetDisplay.cpp`、`PetProtocol.cpp`、`PetState.cpp` 及其对应 `.h`

> Arduino IDE 会自动将 `.ino` 同目录的 `.cpp`/`.h` 文件编译；本工程所有源码已集中放在 `desktop_pet/` 文件夹内。

### 编译上传

1. Tools → Board → 选择 `ESP32S3 Dev Module`
2. Tools → Port → 选择 ESP32 的 COM 口
3. 点击上传 (→)
4. 打开串口监视器 (Tools → Serial Monitor)，设置 115200

---

## 第四步：联调测试 (10分钟)

### 你应该看到的

**ESP32 串口监视器输出：**
```
========================================
  🐾 桌上宠物 ESP32-S3 控制器
  Phase 1: 通信 + 表情系统
========================================

[系统] 初始化完成，等待 STC-B 连接...
[心跳] STC-B 在线
[传感器] 温度ADC=512, 光照ADC=600, 按键=0x00, 标志=0x00
```

**STC-B 数码管：**
- 显示笑脸表情
- L0 指示灯亮起（系统就绪）
- 开机时蜂鸣器"嘀"一声

### 测试清单

| 测试 | 操作 | 预期结果 |
|------|------|---------|
| 通信 | 上电等待 | ESP32 显示 `STC-B 在线` |
| 表情 | ESP32 发命令 | 数码管切换表情 |
| K1 喂食 | 按 STC-B 的 K1 | 数码管显示"吃饭"表情，ESP32 日志显示喂食 |
| K2 玩耍 | 按 STC-B 的 K2 | 数码管显示"玩耍"表情 |
| K3 摸头 | 按 STC-B 的 K3 | 数码管显示"爱心"表情 |
| 拍桌子 | 轻拍 STC-B 板子 | 振动触发，数码管反应 |
| 传感器 | 遮挡光敏电阻 | ESP32 日志中光照值变化 |

---

## 常见问题

### Q: ESP32 收不到 STC-B 的数据？
- 检查 TX-RX 是否交叉连接（STC的TX→ESP32的RX，STC的RX→ESP32的TX）
- 检查 GND 是否连接
- 确认波特率都是 9600
- 用万用表量 EXT 口 TX 引脚，发数据时应有电压跳变

### Q: STC-B 编译报错找不到头文件？
- 确保 BSP 库的头文件路径已添加到 Keil 的 Include Paths
- 检查 `comm.h` 和 `expression.h` 是否在工程的 inc/ 目录中

### Q: 表情显示不对？
- 检查 `decode_table` 是否正确定义（至少 20 个元素）
- 确认 `SetDisplayerArea(0, 7)` 使用了全部 8 位

### Q: 蜂鸣器不响？
- 确认 `BeepInit()` 已被调用
- 检查是否同时在使用 Music 模块（互斥）

---

## 下一步

Phase 1 跑通后，可以继续推进：

1. **表情动画** → 让宠物会眨眼、嘴巴会动
2. **EEPROM存档** → 掉电不丢失宠物状态
3. **WiFi联网** → 接天气API，宠物知道今天下雨
4. **Web页面** → 手机随时看宠物状态
