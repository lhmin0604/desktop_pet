/*
 * beep.c - 蜂鸣器驱动 (SDCC)
 *
 * Timer1: 16-bit auto-reload, 1T mode
 * ISR 里翻转 P3.4 产生方波
 *
 * freq = FOSC / (4 * (65536 - reload))
 * => reload = 65536 - FOSC / (4 * freq)
 */
#include "stc15_sdcc.h"
#include "beep.h"
#define FOSC 11059200L

static volatile unsigned int beep_reload_val;
static volatile unsigned int beep_remain_ms;
static volatile unsigned char beep_state = BEEP_FREE;

void beep_init(void)
{
    P34 = 0;
    TMOD &= 0x0F;          /* Timer1 mode 0: 16-bit auto-reload */
    AUXR |= 0x40;          /* T1x12=1: 1T mode */
    ET1 = 1;
    TR1 = 0;               /* 不启动, 等 beep_set 再开 */
}

void beep_set(unsigned int freq, unsigned int time_10ms)
{
    if (freq == 0 || time_10ms == 0)
    {
        TR1 = 0;
        beep_state = BEEP_FREE;
        return;
    }
    beep_reload_val = (unsigned int)(65536UL - FOSC / (4UL * freq));
    beep_remain_ms = time_10ms * 10;
    beep_state = BEEP_BUSY;
    TH1 = (unsigned char)(beep_reload_val >> 8);
    TL1 = (unsigned char)(beep_reload_val & 0xFF);
    TF1 = 0;
    TR1 = 1;
}

unsigned char beep_status(void)
{
    return beep_state;
}

void beep_tick(void)
{
    /* 在主循环 1ms 检查中调用, 递减计时 */
    if (beep_state == BEEP_BUSY && beep_remain_ms > 0)
    {
        beep_remain_ms--;
        if (beep_remain_ms == 0)
        {
            TR1 = 0;
            P34 = 0;
            beep_state = BEEP_FREE;
        }
    }
}

void timer1_isr(void) __interrupt(3)
{
    P34 = !P34;
    TH1 = (unsigned char)(beep_reload_val >> 8);
    TL1 = (unsigned char)(beep_reload_val & 0xFF);
}
