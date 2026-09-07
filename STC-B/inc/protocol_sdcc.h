/*
 * protocol_sdcc.h - paw_box 协议定义 (SDCC 兼容)
 *
 * 帧格式 (跟 paw_box + ESP32 PetProtocol 完全一致):
 *   [0xAA] [0x55] [ADDR] [CMD] [LEN] [DATA[0..LEN-1]] [CRC8]
 *
 * CRC8: 多项式 0x07, 初值 0x00, 计算范围 ADDR+CMD+LEN+DATA
 * 地址: SLAVE_ADDR=0x01, BROADCAST_ADDR=0xFF
 *
 * 应答约定: STC-B 收到 (CMD & 0x80 == 0) 的主站命令, 回 (CMD | 0x80) 带 payload
 */

#ifndef PROTOCOL_SDCC_H
#define PROTOCOL_SDCC_H

/* ============ 帧格式常量 (跟 ESP32 PetProtocol.h 对齐) ============ */
#define PREAMBLE0        0xAA
#define PREAMBLE1        0x55
#define SLAVE_ADDR       0x01
#define BROADCAST_ADDR   0xFF
#define RESP_FLAG        0x80

#define FRAME_MAX_DATA   48     /* paw_box FRAME_MAX */
#define FRAME_OVERHEAD   7      /* 2*PREAMBLE + ADDR + CMD + LEN + CRC8 */

/* ============ 下行命令 (ESP32 → STC-B, CMD & 0x80 == 0) ============ */
#define CMD_PING         0x00
#define CMD_GET_TEMP     0x01
#define CMD_GET_LIGHT    0x02

#define CMD_LED_SET      0x20   /* 跟 ESP32 PB_CMD_LED_SET 对齐 */
#define CMD_SET_EXPR     0x21   /* 跟 ESP32 PB_CMD_SET_EXPR 对齐 */
#define CMD_BUZZER       0x22
#define CMD_PLAY_SOUND   0x23

#define CMD_QUERY_SENSOR 0x10
#define CMD_QUERY_TIME   0x11
#define CMD_SYS_RESET    0xF1

/* ============ 上行命令 (STC-B → ESP32, CMD & 0x80 != 0) ============ */
#define CMD_SYS_PONG     (CMD_PING   | RESP_FLAG)   /* 0x80 */
#define CMD_SENSOR_TEMP  (CMD_GET_TEMP | RESP_FLAG) /* 0x81, payload[0]=temp */
#define CMD_SENSOR_LIGHT (CMD_GET_LIGHT | RESP_FLAG)/* 0x82, payload[0..1]=light */
#define CMD_EVENT        0xA0                       /* payload[0]=type, [1]=data */
#define CMD_ACK_OK       0xE0                       /* payload[0]=原 CMD */
#define CMD_ACK_FAIL     0xE1                       /* payload[0]=原 CMD, [1]=err */

/* ============ 表情 ID (跟 ESP32 PetProtocol.h 对齐) ============ */
#define EXPR_SMILE       0x00
#define EXPR_LAUGH       0x01
#define EXPR_SAD         0x02
#define EXPR_SLEEPY      0x03
#define EXPR_ANGRY       0x04
#define EXPR_SURPRISE    0x05
#define EXPR_LOVE        0x06
#define EXPR_EAT         0x07
#define EXPR_SICK        0x08
#define EXPR_SLEEP       0x09
#define EXPR_HELLO       0x0A
#define EXPR_PLAY        0x0B
#define EXPR_COUNT       0x0C

/* ============ 事件类型 (跟 ESP32 PetProtocol.h 对齐) ============ */
#define EVENT_KEY        0x01
#define EVENT_NAV        0x02
#define EVENT_VIB        0x03
#define EVENT_HALL       0x04

/* ============ CRC8 (polynomial 0x07) ============ */
unsigned char crc8(const unsigned char *d, unsigned char n);
unsigned char crc8_update(unsigned char crc, const unsigned char *d, unsigned char n);

#endif
