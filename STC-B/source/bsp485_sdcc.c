/*
 * bsp485_sdcc.c - 485 UART2 驱动 (移植自 paw_box, SDCC 兼容)
 *
 * 改动:
 *   - T2L/T2H 使用 T2 定时器作波特率发生器 (跟 paw_box 一致)
 *   - RS485_DE (P3.7) 控制收发方向
 *   - 接收 buffer 16 字节环形队列
 *   - 中断 8 / using(1) 寄存器组 1
 *
 * 关键点 (来自 paw_box 注释):
 *   厂商 485MC 同款: 寄存器组 1
 *   发完最后一个字节后等 3000 cycles (约 1 字节时间) 再拉低 DE,
 *   防止最后停止位还没移出就释放总线
 */

#include "stc15_sdcc.h"
#include "bsp485_sdcc.h"

volatile unsigned char rx_buf[16];
volatile unsigned char rx_head = 0, rx_tail = 0;
static volatile __bit tx_done;

void uart2_init(void)
{
    unsigned int v = 65536UL - FOSC / 4 / BAUD485;
    RS485_DE = 0;
    P_SW2 |= 0x01;       /* S2_S = 1: UART2 切到 P3.6 (RXD2) / P3.7 (TXD2) */
    S2CON = 0x10;        /* mode 1 (8-bit UART), REN=1 */
    T2L = (unsigned char)v;
    T2H = (unsigned char)(v >> 8);
    AUXR |= 0x14;        /* T2R=1 (启动 T2), T2x12=1 (1T 模式) */
    IE2 |= 0x01;         /* ES2 = 1, 使能 UART2 中断 */
    IP2 |= 0x01;         /* PS2 = 1, UART2 中断高优先级 (避免被 ADC 干扰) */
    EA = 1;
}

void uart2_send_buf(const unsigned char *p, unsigned char n)
{
    unsigned char i;
    unsigned int d;

    RS485_DE = 1;        /* 进入发送态 */
    for (i = 0; i < n; i++)
    {
        tx_done = 0;
        S2BUF = p[i];
        while (!tx_done);
    }
    /* TI 在停止位移出前置位, 再驻留约 1 字节时间才释放总线 */
    for (d = 0; d < 3000; d++);
    RS485_DE = 0;        /* 切回接收态 */
}

__bit rx_get(unsigned char *d)
{
    if (rx_head == rx_tail) return 0;
    *d = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) & 0x0F;
    return 1;
}

void uart2_isr(void) __interrupt(8) __using(1)   /* 厂商 485MC 同款: 寄存器组 1 */
{
    if (S2CON & 0x01)
    {
        S2CON &= ~0x01;
        rx_buf[rx_head] = S2BUF;
        rx_head = (rx_head + 1) & 0x0F;
    }
    if (S2CON & 0x02)
    {
        S2CON &= ~0x02;
        tx_done = 1;
    }
}
