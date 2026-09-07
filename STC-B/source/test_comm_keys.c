/* test_comm_keys.c - 最小测试固件: UART1 debug + RS485 通信 + 按键
 * 不初始化数码管/蜂鸣器/表情, 排除显示部分干扰
 */
#include "stc15_sdcc.h"

/* ============ UART1 debug (P3.1 TX, 115200) ============ */
__sfr __at (0x98) SCON1;
__sfr __at (0x99) SBUF1;
__sbit __at (0x99) TI1;

static void uart1_init(void) {
    SCON1 = 0x40;
    BRT = (unsigned char)(256U - 11059200UL / 2UL / 115200UL);
    AUXR |= 0x04;
    AUXR |= 0x10;
    AUXR |= 0x01;
    TI1 = 1;
}

static void uart1_putc(char c) {
    while (!TI1);
    TI1 = 0;
    SBUF1 = (unsigned char)c;
}

static void uart1_puts(const char *s) {
    while (*s) uart1_putc(*s++);
}

static void uart1_hex8(unsigned char v) {
    const char hex[] = "0123456789ABCDEF";
    uart1_putc(hex[(v >> 4) & 0x0F]);
    uart1_putc(hex[v & 0x0F]);
}

/* ============ UART2 RS485 (P3.6 RXD2 / P3.7 TXD2, T2 baud 9600) ============ */
__sfr __at (0x9A) S2CON;
__sfr __at (0x9B) S2BUF;
__sbit __at (0xB7) RS485_DE;

static volatile unsigned char rx_buf[16];
static volatile unsigned char rx_head = 0, rx_tail = 0;

static void uart2_init(void) {
    unsigned int v = 65536UL - 11059200UL / 4 / 9600;
    RS485_DE = 0;
    P_SW2 |= 0x01;       /* S2_S = 1: P4.6(RXD2)/P4.7(TXD2) */  // 使用 P4.6(RXD2)/P4.7(TXD2)  // 注释掉，使用默认 P1.0/P1.1
    S2CON = 0x10;
    T2L = (unsigned char)v;
    T2H = (unsigned char)(v >> 8);
    AUXR |= 0x14;
    IE2 |= 0x01;
    EA = 1;
}

static void uart2_send(const unsigned char *p, unsigned char n) {
    unsigned char i;
    RS485_DE = 1;
    for (i = 0; i < n; i++) {
        S2BUF = p[i];
        while (!(S2CON & 0x02));
        S2CON &= ~0x02;
    }
    { unsigned int d; for (d = 0; d < 3000; d++); }
    RS485_DE = 0;
}

void uart2_isr(void) __interrupt(8) __using(1) {
    if (S2CON & 0x01) {
        S2CON &= ~0x01;
        rx_buf[rx_head] = S2BUF;
        rx_head = (rx_head + 1) & 0x0F;
    }
    if (S2CON & 0x02) {
        S2CON &= ~0x02;
    }
}

/* ============ Timer0 1ms ============ */
static volatile unsigned int tick_ms = 0;

static void timer0_init(void) {
    TMOD &= 0xF0;
    AUXR |= 0x80;
    TH0 = 0xD4;
    TL0 = 0xCD;
    TR0 = 1;
    ET0 = 1;
    EA = 1;
}

void timer0_isr(void) __interrupt(1) {
    tick_ms++;
}

/* ============ 按键 (K1=P3.2, K2=P3.3, K3=P1.7) ============ */
static unsigned char key_prev[3] = {1, 1, 1};
static unsigned char key_debounce[3] = {0, 0, 0};

static void keys_init(void) { P3M0 &= ~0x0C; P3M1 |= 0x0C; P3 |= 0x0C; P1M0 &= ~0x80; P1M1 |= 0x80; P1 |= 0x80; } static void keys_scan(void) {
    unsigned char i;
    unsigned char val;
    for (i = 0; i < 3; i++) {
        if (i == 0) val = P32; else if (i == 1) val = P33; else val = P17;
        if (val != key_prev[i]) {
            key_debounce[i] = 0;
            key_prev[i] = val;
        } else if (key_debounce[i] < 20) {
            key_debounce[i]++;
            if (key_debounce[i] == 10 && val == 0) {
                uart1_puts("[KEY] K");
                uart1_putc('1' + i);
                uart1_puts(" pressed\r\n");
            }
        }
    }
}

/* ============ 主程序 ============ */
void main(void) {
    uart1_init();
    uart2_init(); keys_init();
    timer0_init();

    uart1_puts("\r\n[TEST] STC-B comm+keys test\r\n");
    uart1_puts("[TEST] UART1 OK @ 115200\r\n");
    uart1_puts("[TEST] UART2 OK @ 9600 (P3.6/P3.7)\r\n");
    uart1_puts("[TEST] Keys: K1=P3.2 K2=P3.3 K3=P1.7\r\n");
    uart1_puts("[TEST] Waiting...\r\n");

    while (1) {
        unsigned char b;
        unsigned char got = 0;

        /* 检查 UART2 接收 */
        while (rx_head != rx_tail) {
            b = rx_buf[rx_tail];
            rx_tail = (rx_tail + 1) & 0x0F;
            uart1_puts("[RX] 0x");
            uart1_hex8(b);
            uart1_puts("\r\n");
            got = 1;
        }

        /* 按键扫描 (每 5ms) */
        if (tick_ms >= 5) {
            tick_ms = 0;
            keys_scan();
        }

        /* 心跳 (每 1000ms) */
        {
            static unsigned int cnt = 0;
            cnt++;
            if (cnt >= 1000) {
                cnt = 0;
                uart1_puts("[ALIVE]\r\n");
            }
        }
    }
}
