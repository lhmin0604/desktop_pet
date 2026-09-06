/*
 * uart2_ext.c - UART2 驱动 (SDCC)
 *
 * UART2 在 P1.0(RXD2)/P1.1(TXD2), 经200R到EXT口, 接ESP32.
 * 使用独立波特率发生器 BRT, 1T模式.
 * 接收: 64字节环形缓冲区, ISR写入.
 */
#include "stc15_sdcc.h"
#include "uart2_ext.h"

#define RX_BUF_SIZE 64

static volatile unsigned char rx_buf[RX_BUF_SIZE];
static volatile unsigned char rx_head = 0;
static volatile unsigned char rx_tail = 0;
static volatile unsigned char tx_busy_flag = 0;

void uart2_init(void)
{
    P_SW2 &= ~0x01;           /* S2_S=0: UART2 -> P1.0/P1.1 */
    S2CON = 0x10;             /* mode 1 (8-bit UART), REN=1 */
    BRT = (unsigned char)(256UL - FOSC / 4L / BAUD);
    AUXR |= 0x01;             /* S2_BRTx: 用BRT做UART2波特率 */
    AUXR |= 0x10;             /* BRTx12=1: BRT 1T模式 */
    AUXR |= 0x04;             /* BRTEN=1: 启动BRT */
    IE2  |= 0x01;             /* ES2=1: UART2中断使能 */
    EA = 1;
}

void uart2_send_buf(const unsigned char *p, unsigned char n)
{
    unsigned char i;
    for (i = 0; i < n; i++)
    {
        tx_busy_flag = 0;
        S2BUF = p[i];
        while (!tx_busy_flag);
    }
}

unsigned char uart2_rx_available(void)
{
    return (rx_head != rx_tail);
}

unsigned char uart2_rx_get(void)
{
    unsigned char d;
    if (rx_head == rx_tail) return 0;
    d = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) & (RX_BUF_SIZE - 1);
    return d;
}

void uart2_isr(void) __interrupt(8) __using(1)
{
    if (S2CON & 0x01)         /* RI */
    {
        S2CON &= ~0x01;
        rx_buf[rx_head] = S2BUF;
        rx_head = (rx_head + 1) & (RX_BUF_SIZE - 1);
    }
    if (S2CON & 0x02)         /* TI */
    {
        S2CON &= ~0x02;
        tx_busy_flag = 1;
    }
}
