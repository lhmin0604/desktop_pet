/*************************************************************
 * 桌上宠物 - ESP32-S3 主控程序 (Arduino)
 * Phase 1: 通信 + 宠物状态机
 *
 * 硬件: ESP32-S3-BOX-3B + BOX-3-DOCK
 * 连接: GPIO43(TX2) → STC-B EXT RXD  (3.3V, 直连)
 *        GPIO44(RX2) ← STC-B EXT TXD  (5V→3.3V 电平转换)
 *        GND → GND
 * 波特率: 9600 bps
 *************************************************************/

#include <Arduino.h>
#include <Wire.h>
#include "PetProtocol.h"
#include "PetState.h"
#include "PetDisplay.h"

/* ==================== 引脚定义 ==================== */
#define ESP_TX2_PIN   43    /* ESP32 TX2 → 485模块DI (3.3V直连) */
#define ESP_RX2_PIN   44    /* ESP32 RX2 ← 485模块RO (需3.3V TTL, 模块RO走3.3V电平) */
#define ESP_485_DE    38    /* ESP32 GPIO38 → 485模块DE+RE (Pmod左上) */
#define SERIAL_BAUD   9600

/* 调试串口 */
#define DEBUG_BAUD    115200

/* 触摸 I2C 引脚 (BOX-3B TT21100/GT911) */
#define TOUCH_SDA_PIN  8
#define TOUCH_SCL_PIN  18

/* ==================== I2C 扫描 (debug) ====================
 * 触摸不响应时先扫一下看是什么 IC 什么地址
 *
 * 先用当前 (SDA=8, SCL=18) + 400kHz 跑一遍, 如果没设备就试常见组合:
 *   (8,18) (18,8) (9,39) (39,9), 各 100k 和 400k
 * 输出每个找到的设备 + chip ID (0x8140 处读 4 字节, TT21100 应为 "TT21", GT911 应为 0x3911)
 */
void scanI2C() {
    /* 候选引脚组合 — 涵盖 BOX-3 / BOX-3B 各批次 / SDA-SCL 调换 */
    struct { int sda, scl; const char* label; } pins[] = {
        {  8, 18, "SDA=8  SCL=18  (BOX-3 默认)" },
        { 18,  8, "SDA=18 SCL=8   (反序)" },
        {  9, 39, "SDA=9  SCL=39  (备选)" },
        { 39,  9, "SDA=39 SCL=9   (反序)" },
    };
    uint32_t freqs[] = { 100000UL, 400000UL };

    Serial.println("\n========== I2C 扫描 ==========");
    Serial.println("  尝试 4 种引脚组合 × 2 种频率, 找第一个能 ACK 的");

    bool found_any = false;
    for (auto& p : pins) {
        for (auto f : freqs) {
            Serial.printf("\n  --- 试 %s @ %d kHz ---\n", p.label, f / 1000);
            Wire.begin(p.sda, p.scl, f);

            uint8_t addrs[16];
            int n = 0;
            for (uint8_t a = 0x01; a < 0x7F; a++) {
                Wire.beginTransmission(a);
                uint8_t err = Wire.endTransmission();
                if (err == 0 && n < 16) addrs[n++] = a;
            }

            if (n == 0) {
                Serial.println("  无设备 ACK");
                continue;
            }

            found_any = true;
            Serial.printf("  ✅ 找到 %d 个设备: ", n);
            for (int i = 0; i < n; i++) Serial.printf("0x%02X ", addrs[i]);
            Serial.println();

            /* 读每个设备的 chip ID */
            for (int i = 0; i < n; i++) {
                uint8_t a = addrs[i];
                uint8_t reg[2] = { 0x81, 0x40 };
                Wire.beginTransmission(a);
                Wire.write(reg, 2);
                uint8_t err = Wire.endTransmission(false);
                if (err != 0) { Serial.printf("    @0x%02X: 无法写寄存器\n", a); continue; }
                Wire.requestFrom(a, (uint8_t)4);
                if (Wire.available() != 4) {
                    Serial.printf("    @0x%02X: 读不到 4 字节 (avail=%d)\n", a, Wire.available());
                    continue;
                }
                uint8_t id[4];
                for (int k = 0; k < 4; k++) id[k] = Wire.read();

                const char* ic = "?";
                if (id[0] == 0x54 && id[1] == 0x54) ic = "TT21100 (TT21)";
                else if (id[0] == 0x39 && id[1] == 0x31) ic = "GT911/GT9110";
                else if (id[0] == 0x00 && id[1] == 0x00) ic = "(全0, 其它寄存器布局)";

                Serial.printf("    @0x%02X chip ID: %02X %02X %02X %02X  ASCII='%c%c%c%c'  → %s\n",
                    a, id[0], id[1], id[2], id[3],
                    (id[0] >= 32 && id[0] < 127) ? id[0] : '.',
                    (id[1] >= 32 && id[1] < 127) ? id[1] : '.',
                    (id[2] >= 32 && id[2] < 127) ? id[2] : '.',
                    (id[3] >= 32 && id[3] < 127) ? id[3] : '.',
                    ic);
            }
        }
    }

    if (!found_any) {
        Serial.println("\n  ❌ 8 种组合全部无 ACK!");
        Serial.println("  硬件排查清单:");
        Serial.println("   1) BOX-3B 排线/DOCK 是否插紧 (触摸走 DOCK P1)");
        Serial.println("   2) 触摸 IC 是否有 3.3V 供电 (万用表量触摸 IC 的 VCC)");
        Serial.println("   3) 触摸 IC 的 SDA/SCL 上是否有 4.7k 上拉到 3.3V");
        Serial.println("   4) 触摸 IC 是不是坏了 / 是不是另一型号 (NS2009 / CSTxxx)");
    }
    Serial.println("\n================================\n");
}

/* ==================== 全局对象 ==================== */
PetProtocol protocol;       /* 通信协议处理器 */
PetState pet;               /* 宠物状态机 */
PetDisplay display;         /* BOX-3B 屏幕表情 */

/* 屏幕表情渲染刷新计时（心情/属性/眨眼任一变化时重画，render 内部短路） */
unsigned long last_face_render = 0;

/* ==================== 定时器 ==================== */
unsigned long last_sensor_query = 0;
unsigned long last_ping = 0;
unsigned long last_state_update = 0;

#define SENSOR_QUERY_INTERVAL   2000    /* 每2秒查询传感器 */
#define PING_INTERVAL           5000    /* 每5秒心跳 */
#define STATE_UPDATE_INTERVAL   1000    /* 每秒更新状态 */

/* 连接状态 */
bool stc_connected = false;
int ping_fail_count = 0;
#define PING_TIMEOUT  3     /* 连续3次无回应视为离线 */

/* ==================== 命令回调处理 ==================== */

/* 收到 STC-B 的传感器数据 */
void onSensorReport(uint16_t temp, uint16_t light,
                    uint8_t buttons, uint8_t flags) {
    pet.updateSensor(temp, light, buttons, flags);
    stc_connected = true;
    ping_fail_count = 0;

    Serial.printf("[传感器] 温度ADC=%d, 光照ADC=%d, 按键=0x%02X, 标志=0x%02X\n",
                  temp, light, buttons, flags);
}

/* 收到 STC-B 的事件上报 */
void onEvent(uint8_t event_type, uint8_t event_data) {
    stc_connected = true;
    ping_fail_count = 0;

    switch (event_type) {
        case EVENT_KEY:
            Serial.printf("[事件] 按键 K%d 按下\n", event_data);
            pet.onButtonPress(event_data);
            break;
        case EVENT_NAV:
            Serial.printf("[事件] 导航按键 方向=%d\n", event_data);
            pet.onNavPress(event_data);
            break;
        case EVENT_VIB:
            Serial.println("[事件] 检测到振动！");
            pet.onVibration();
            break;
        case EVENT_HALL:
            Serial.printf("[事件] 霍尔 %s\n",
                          event_data ? "磁铁靠近" : "磁铁离开");
            pet.onHall(event_data);
            break;
    }
}

/* 收到 ACK */
void onAck(uint8_t cmd, bool success) {
    if (success) {
        Serial.printf("[ACK] CMD=0x%02X 成功\n", cmd);
    } else {
        Serial.printf("[ACK] CMD=0x%02X 失败\n", cmd);
    }
}

/* 收到心跳回应 */
void onPong() {
    stc_connected = true;
    ping_fail_count = 0;
    Serial.println("[心跳] STC-B 在线");
}

/* ==================== PC USB 串口命令菜单 ====================
 * 波特率 115200，行尾选 "No line ending"（按字符识别，无需回车）
 * 命令:
 *   1-8  切心情 (1开心 2普通 3饥饿 4困倦 5生气 6生病 7超开心 8恋爱)
 *   f    喂食 (K1, ACT_EAT, 饱食+20 快乐+5 精力+10)
 *   p    玩耍 (K2, ACT_PLAY, 快乐+15 精力-5)
 *   s    摸头 (K3, ACT_STROKE, 好感+10 快乐+8 精力+5)
 *   u    站立 (导航上, ACT_STAND_UP)
 *   v    拍桌 (ACT_VIBRATION)
 *   i    重扫 I2C 总线 (触摸 debug)
 *   h/?  打印本菜单
 */
void printMoodMenu() {
    Serial.println("\n===== 命令菜单 =====");
    Serial.println("  1-8  切心情  1开心 2普通 3饥饿 4困倦");
    Serial.println("                 5生气 6生病 7超开心 8恋爱");
    Serial.println("  f 喂食    p 玩耍    s 摸头    u 站立");
    Serial.println("  v 拍桌    i 重扫I2C  h / ?  打印本菜单");
    Serial.println("====================");
}

void handleSerialCommand(char c) {
    if (c >= '1' && c <= '8') {
        PetMood m = (PetMood)(c - '1');
        pet.setMood(m);
        Serial.printf("[命令] 心情 → %d\n", m);
        display.render(pet.getMood(), pet.getAction(), pet.getStats(), millis());
    } else if (c == 'f' || c == 'F') {
        Serial.println("[命令] 喂食");
        pet.onButtonPress(1);
    } else if (c == 'p' || c == 'P') {
        Serial.println("[命令] 玩耍");
        pet.onButtonPress(2);
    } else if (c == 's' || c == 'S') {
        Serial.println("[命令] 摸头");
        pet.onButtonPress(3);
    } else if (c == 'u' || c == 'U') {
        Serial.println("[命令] 站立");
        pet.onNavPress(NAV_UP);
    } else if (c == 'v' || c == 'V') {
        Serial.println("[命令] 拍桌子");
        pet.onVibration();
    } else if (c == 'i' || c == 'I') {
        Serial.println("[命令] 重扫 I2C");
        scanI2C();
    } else if (c == 'h' || c == 'H' || c == '?') {
        printMoodMenu();
    } else if (c == '\n' || c == '\r' || c == ' ') {
        /* 忽略回车/换行/空格 */
    } else {
        Serial.printf("[?] 未知命令 '%c' (0x%02X)，按 h 看帮助\n", c, (uint8_t)c);
    }
}

/* ==================== Arduino Setup ==================== */
void setup() {
    /* 调试串口 (USB) — ESP32-S3 原生 USB CDC 必须等枚举完成才能 println */
    Serial.begin(DEBUG_BAUD);
    delay(1500);   /* 给 Windows 枚举 USB CDC 设备的时间，否则前几行可能丢失 */
    Serial.println("\n========================================");
    Serial.println("  🐾 桌上宠物 ESP32-S3 控制器");
    Serial.println("  Phase 1.8: 8 心情 + 4 动作完整身体矢量图");
    Serial.println("========================================\n");
    Serial.println("[1/4] 调试串口 OK (115200)");

    /* 通信串口 (连接 STC-B via RS485) */
    Serial2.begin(SERIAL_BAUD, SERIAL_8N1, ESP_RX2_PIN, ESP_TX2_PIN);
    Serial.println("[2/4] Serial2 (UART2 → 485) 已开 9600");

    /* 初始化协议处理器 + RS485 DE 控制 */
    protocol.begin(&Serial2, ESP_485_DE);
    protocol.onSensorReport(onSensorReport);
    protocol.onEvent(onEvent);
    protocol.onAck(onAck);
    protocol.onPong(onPong);
    Serial.println("[3/4] 协议回调已注册 (paw_box 协议 + 485 DE)");

    /* 初始化宠物状态机 */
    pet.begin(&protocol);
    Serial.println("[4/4] 状态机就绪");

    /* 0. I2C 触摸扫描 (debug) — 触摸不响应时确认硬件和地址 */
    scanI2C();

    /* 点亮 BOX-3B 屏幕,默认状态由 PetState::evaluateMood() 根据初始属性决定 */
    Serial.println("[屏幕] 初始化中...");
    display.begin();
    display.render(pet.getMood(), pet.getAction(), pet.getStats(), millis());
    Serial.println("[屏幕] OK");
    printMoodMenu();

    Serial.println("\n[系统] 初始化完成，等待 STC-B 连接...");
    Serial.println("[系统] 请确认 STC-B 已通过 485 接口连接 (板上 MAX485 ↔ ESP32 485模块)");
    Serial.println("[提示] 串口命令: 1-8 切心情 / f 喂食 / p 玩耍 / s 摸头 / u 站立 / v 拍桌 / i 重扫I2C / h 帮助\n");

    /* 等待 STC-B 上线 */
    delay(2000);
    protocol.sendPing();
}

/* ==================== Arduino Loop ==================== */
void loop() {
    unsigned long now = millis();

    /* 0. 处理 PC USB 串口命令（无 STC 时也能调试） */
    if (Serial.available()) {
        handleSerialCommand((char)Serial.read());
    }

    /* 1. 处理串口接收 */
    protocol.process();

    /* 2. 定时查询传感器 */
    if (now - last_sensor_query > SENSOR_QUERY_INTERVAL) {
        last_sensor_query = now;
        protocol.sendQuerySensor();
    }

    /* 3. 心跳检测 */
    if (now - last_ping > PING_INTERVAL) {
        last_ping = now;
        protocol.sendPing();
        ping_fail_count++;
        if (ping_fail_count > PING_TIMEOUT) {
            if (stc_connected) {
                Serial.println("[警告] STC-B 似乎离线了！");
                stc_connected = false;
            }
        }
    }

    /* 4. 宠物状态更新 */
    if (now - last_state_update > STATE_UPDATE_INTERVAL) {
        last_state_update = now;
        pet.update();
    }

    /* 5. 屏幕表情随心情/动作/属性刷新（80ms 节流，render 内部对未变化短路） */
    if (now - last_face_render > 80) {
        last_face_render = now;
        display.render(pet.getMood(), pet.getAction(), pet.getStats(), now);
    }

    /* 6. 触摸处理 — 3 区域映射:
     *   y= 24-130  (猫头上半)  → STROKE (K3, 摸头)
     *   y=130-200  (猫身下半)  → PLAY   (K2, 逗猫)
     *   y=200-240  (底部 40px)  → EAT    (K1, 喂食)
     *   y=  0- 24  (状态栏)    → 忽略
     * 用 getTouch() 已去抖,每次按下只触发一次 onButtonPress */
    int tx, ty;
    if (display.getTouch(&tx, &ty)) {
        if (ty < 24) {
            /* 状态栏,忽略 */
        } else if (ty < 130) {
            Serial.printf("[触摸] (%d,%d) 摸头 → STROKE\n", tx, ty);
            pet.onButtonPress(3);
        } else if (ty < 200) {
            Serial.printf("[触摸] (%d,%d) 逗猫 → PLAY\n", tx, ty);
            pet.onButtonPress(2);
        } else {
            Serial.printf("[触摸] (%d,%d) 喂食 → EAT\n", tx, ty);
            pet.onButtonPress(1);
        }
    }
}
