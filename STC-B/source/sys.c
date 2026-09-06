/*
 * sys.c - 系统定时器 (SDCC)
 *
 * Timer0: 1ms 中断, 1T 模式, 11.0592MHz
 *   reload = 65536 - 11059200/1000 = 65536 - 11059 = 54477 = 0xD4CD
 */
#include "stc15_sdcc.h"
#include "sys.h"

volatile unsigned int sys_tick_ms = 0;
volatile unsigned char sys_flag_10ms = 0;
volatile unsigned char sys_flag_100ms = 0;
volatile unsigned char sys_flag_1000ms = 0;

static unsigned char cnt_10ms = 0;
static unsigned char cnt_100ms = 0;
static unsigned int  cnt_1000ms = 0;

void sys_init(void)
{
    TMOD &= 0xF0;          /* Timer0 mode 0: 16-bit auto-reload */
    AUXR |= 0x80;          /* T0x12=1: 1T 模式 */
    TH0 = 0xD4;
    TL0 = 0xCD;            /* reload = 0xD4CD = 54477 */
    TR0 = 1;
    ET0 = 1;
    EA  = 1;
}

void timer0_isr(void) __interrupt(1)
{
    sys_tick_ms++;

    cnt_10ms++;
    if (cnt_10ms >= 10)
    {
        cnt_10ms = 0;
        sys_flag_10ms = 1;
        cnt_100ms++;
        if (cnt_100ms >= 10)
        {
            cnt_100ms = 0;
            sys_flag_100ms = 1;
            cnt_1000ms++;
            if (cnt_1000ms >= 10)
            {
                cnt_1000ms = 0;
                sys_flag_1000ms = 1;
            }
        }
    }
}
