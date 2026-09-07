/*
 * display.c - 数码管 + LED 驱动 (SDCC)
 *
 * 数码管: P0 = 段码, P2.0-P2.2 = 74HC138 位选 (active LOW)
 * LED:    P2.3 = ULN2003 驱动 (active HIGH)
 *
 * display_refresh() 每 2ms 调用一次 (在 main loop 里)
 */
#include "stc15_sdcc.h"
#include "display.h"

/* 不加 = {0} 初始化, 走 BSS (SDCC BSS 清零工作正常, XINIT 复制有 bug).
 * 真正关键的数据 (face_table) 改成 const 放 CODE 区. */
unsigned char disp_segments[8];
unsigned char disp_led;
static unsigned char disp_digit;

void display_init(void)
{
    unsigned char i;
    for (i = 0; i < 8; i++)
        disp_segments[i] = 0x00;

    P0 = 0x00;
    P2 = 0x87;              /* P2.3=1(LED off initially), 138=111(all off) */
    P0M0 = 0xFF;            /* P0 push-pull output (段码) */
    P0M1 = 0x00;
}

void display_set_seg(unsigned char pos, unsigned char seg)
{
    if (pos < 8)
        disp_segments[pos] = seg;
}

void display_set_all(unsigned char s0, unsigned char s1, unsigned char s2,
                     unsigned char s3, unsigned char s4, unsigned char s5,
                     unsigned char s6, unsigned char s7)
{
    disp_segments[0] = s0; disp_segments[1] = s1;
    disp_segments[2] = s2; disp_segments[3] = s3;
    disp_segments[4] = s4; disp_segments[5] = s5;
    disp_segments[6] = s6; disp_segments[7] = s7;
}

void display_set_led(unsigned char val)
{
    disp_led = val;
}

void display_refresh(void)
{
    P0 = 0x00;              /* blank during transition */
    P2 = 0x87;              /* all digits off, keep P2.3=1 */

    P0 = disp_segments[disp_digit];
    P2 = (0x80 | (disp_led ? 0x08 : 0x00) | (disp_digit & 0x07));

    disp_digit = (disp_digit + 1) & 0x07;
}
