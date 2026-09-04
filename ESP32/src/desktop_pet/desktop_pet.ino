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
#include "PetProtocol.h"
#include "PetState.h"
#include "PetDisplay.h"

/* ==================== 引脚定义 ==================== */
#define ESP_TX2_PIN   43    /* ESP32 TX2 → STC-B EXT_RXD (3.3V直连) */
#define ESP_RX2_PIN   44    /* ESP32 RX2 ← STC-B EXT_TXD (需5V→3.3V电平转换) */
#define SERIAL_BAUD   9600

/* 调试串口 */
#define DEBUG_BAUD    115200

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
 *   f    喂食 (K1, 饱食+20 快乐+5)
 *   p    玩耍 (K2, 快乐+15 精力-5)
 *   s    摸头 (K3, 好感+10 快乐+8 心情→恋爱)
 *   v    拍桌子 (振动事件)
 *   h/?  打印本菜单
 */
void printMoodMenu() {
    Serial.println("\n===== 命令菜单 =====");
    Serial.println("  1-8  切心情  1开心 2普通 3饥饿 4困倦");
    Serial.println("                 5生气 6生病 7超开心 8恋爱");
    Serial.println("  f 喂食    p 玩耍    s 摸头    v 拍桌");
    Serial.println("  h / ?  打印本菜单");
    Serial.println("====================");
}

void handleSerialCommand(char c) {
    if (c >= '1' && c <= '8') {
        PetMood m = (PetMood)(c - '1');
        pet.setMood(m);
        Serial.printf("[命令] 心情 → %d\n", m);
        display.render(pet.getMood(), pet.getStats(), millis());
    } else if (c == 'f' || c == 'F') {
        Serial.println("[命令] 喂食");
        pet.onButtonPress(1);
    } else if (c == 'p' || c == 'P') {
        Serial.println("[命令] 玩耍");
        pet.onButtonPress(2);
    } else if (c == 's' || c == 'S') {
        Serial.println("[命令] 摸头");
        pet.onButtonPress(3);
    } else if (c == 'v' || c == 'V') {
        Serial.println("[命令] 拍桌子");
        pet.onVibration();
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
    Serial.println("  Phase 1.7: SLEEPY 完整身体 + 状态栏");
    Serial.println("========================================\n");
    Serial.println("[1/4] 调试串口 OK (115200)");

    /* 通信串口 (连接 STC-B) */
    Serial2.begin(SERIAL_BAUD, SERIAL_8N1, ESP_RX2_PIN, ESP_TX2_PIN);
    Serial.println("[2/4] Serial2 (UART2 → STC-B) 已开 9600");

    /* 初始化协议处理器 */
    protocol.begin(&Serial2);
    protocol.onSensorReport(onSensorReport);
    protocol.onEvent(onEvent);
    protocol.onAck(onAck);
    protocol.onPong(onPong);
    Serial.println("[3/4] 协议回调已注册");

    /* 初始化宠物状态机 */
    pet.begin(&protocol);
    Serial.println("[4/4] 状态机就绪");

    /* 点亮 BOX-3B 屏幕，先画一只睡觉的小猫（默认状态） */
    Serial.println("[屏幕] 初始化中...");
    display.begin();
    pet.setMood(MOOD_SLEEPY);
    display.render(pet.getMood(), pet.getStats(), millis());
    Serial.println("[屏幕] OK");
    printMoodMenu();

    Serial.println("\n[系统] 初始化完成，等待 STC-B 连接...");
    Serial.println("[系统] 请确认 STC-B 已通过 EXT 口连接");
    Serial.println("[提示] 串口命令: 1-8 切心情 / f 喂食 / p 玩耍 / s 摸头 / v 拍桌 / h 帮助\n");

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

    /* 5. 屏幕表情随心情/属性/眨眼刷新（80ms 节流，render 内部对未变化短路） */
    if (now - last_face_render > 80) {
        last_face_render = now;
        display.render(pet.getMood(), pet.getStats(), now);
    }
}
