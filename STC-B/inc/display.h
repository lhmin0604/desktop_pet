/*
 * display.h - 数码管 + LED 驱动 (SDCC)
 *
 * 硬件: 8位共阴数码管, 段码 P0.0-P0.7, 位选 P2.0-P2.2 (74HC138)
 *       LED使能 P2.3 (ULN2003)
 */
#ifndef DISPLAY_H
#define DISPLAY_H

extern unsigned char disp_segments[8];
extern unsigned char disp_led;

void display_init(void);
void display_refresh(void);
void display_set_seg(unsigned char pos, unsigned char seg);
void display_set_all(unsigned char s0, unsigned char s1, unsigned char s2,
                     unsigned char s3, unsigned char s4, unsigned char s5,
                     unsigned char s6, unsigned char s7);
void display_set_led(unsigned char val);

#endif
