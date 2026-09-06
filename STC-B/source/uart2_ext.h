/*
 * uart2_ext.h - UART2 驱动 (SDCC, P1.0/P1.1 EXT口)
 */
#ifndef UART2_EXT_H
#define UART2_EXT_H

#define FOSC    11059200L
#define BAUD    9600L

void uart2_init(void);
void uart2_send_buf(const unsigned char *p, unsigned char n);
unsigned char uart2_rx_available(void);
unsigned char uart2_rx_get(void);

#endif
