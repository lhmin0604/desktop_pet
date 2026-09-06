/*
 * bsp485_sdcc.h - 485 UART2 驱动头文件 (移植自 paw_box, SDCC 兼容)
 */

#ifndef BSP485_SDCC_H
#define BSP485_SDCC_H

#define FOSC        11059200L
#define BAUD485     9600

extern volatile unsigned char rx_buf[16];
extern volatile unsigned char rx_head;
extern volatile unsigned char rx_tail;

void uart2_init(void);
void uart2_send_buf(const unsigned char *p, unsigned char n);
__bit rx_get(unsigned char *d);

#endif
