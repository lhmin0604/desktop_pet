/*
 * protocol_esp32.h - ESP32 协议定义 (SDCC 兼容)
 *
 * 帧格式 (跟 ESP32 PetProtocol 一致):
 *   [0xAA] [CMD] [LEN] [DATA[0..LEN-1]] [CHK]
 *
 * CHK: HEAD ^ CMD ^ LEN ^ DATA[0..LEN-1]
 */

#ifndef PROTOCOL_ESP32_H
#define PROTOCOL_ESP32_H

#define FRAME_HEAD      0xAA
#define FRAME_MAX_DATA  16
#define FRAME_OVERHEAD  4

/* 下行命令 (ESP32 -> STC-B) */
#define CMD_SET_EXPRESSION   0x01
#define CMD_SET_CUSTOM_FACE  0x02
#define CMD_SET_LED          0x03
#define CMD_SET_BUZZER       0x04
#define CMD_SET_MOTOR        0x05
#define CMD_PLAY_SOUND       0x06
#define CMD_SET_ALL          0x07
#define CMD_QUERY_SENSOR     0x10
#define CMD_QUERY_TIME       0x11
#define CMD_SYS_PING         0xF0
#define CMD_SYS_RESET        0xF1

/* 上行命令 (STC-B -> ESP32) */
#define CMD_SENSOR_REPORT    0x20
#define CMD_TIME_REPORT      0x21
#define CMD_EVENT_REPORT     0x22
#define CMD_ACK_OK           0xE0
#define CMD_ACK_FAIL         0xE1
#define CMD_SYS_PONG         0xF0

/* 表情ID */
#define EXPR_SMILE      0x00
#define EXPR_LAUGH      0x01
#define EXPR_SAD        0x02
#define EXPR_SLEEPY     0x03
#define EXPR_ANGRY      0x04
#define EXPR_SURPRISE   0x05
#define EXPR_LOVE       0x06
#define EXPR_EAT        0x07
#define EXPR_SICK       0x08
#define EXPR_SLEEP      0x09
#define EXPR_HELLO      0x0A
#define EXPR_PLAY       0x0B
#define EXPR_COUNT      0x0C

/* 音效ID */
#define SOUND_SHORT     0x01
#define SOUND_HAPPY     0x02
#define SOUND_SAD       0x03
#define SOUND_ALARM     0x04
#define SOUND_EAT       0x05
#define SOUND_SLEEP     0x06
#define SOUND_WAKE      0x07
#define SOUND_LOVE      0x08

/* 事件类型 */
#define EVENT_KEY       0x01
#define EVENT_NAV       0x02
#define EVENT_VIB       0x03
#define EVENT_HALL      0x04

/* 接收帧结构 */
typedef struct {
    unsigned char cmd;
    unsigned char len;
    unsigned char data[FRAME_MAX_DATA];
} CommFrame;

#endif
