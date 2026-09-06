/*
 * protocol_sdcc.h - paw_box 协议定义 (SDCC 兼容)
 *
 * 帧格式 (跟 paw_box 完全一致):
 *   [0xAA] [0x55] [ADDR] [CMD] [LEN] [DATA[0..LEN-1]] [CRC8]
 *
 * CRC8: 多项式 0x07, 初值 0x00, 计算范围 ADDR+CMD+LEN+DATA
 * 地址: SLAVE_ADDR=0x01, BROADCAST_ADDR=0xFF
 */

#ifndef PROTOCOL_SDCC_H
#define PROTOCOL_SDCC_H

#define PREAMBLE0       0xAA
#define PREAMBLE1       0x55
#define SLAVE_ADDR      0x01
#define BROADCAST_ADDR  0xFF

/* Commands */
#define CMD_PING        0x00
#define CMD_GET_TEMP    0x01
#define CMD_GET_LIGHT   0x02
#define CMD_SET_EXPR    0x20   /* 设置表情 (我们扩展) */
#define CMD_LED_SET     0x21
#define RESP_FLAG       0x80

/* CRC8 (polynomial 0x07) */
unsigned char crc8(const unsigned char *d, unsigned char n);
unsigned char crc8_update(unsigned char crc, const unsigned char *d, unsigned char n);

#endif
