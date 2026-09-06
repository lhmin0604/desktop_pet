/*
 * beep.h - 蜂鸣器驱动 (SDCC)
 *
 * 蜂鸣器: P3.4, Timer1 翻转产生方波
 */
#ifndef BEEP_H
#define BEEP_H

#define BEEP_FREE   0
#define BEEP_BUSY   1

void beep_init(void);
void beep_set(unsigned int freq, unsigned int time_10ms);
unsigned char beep_status(void);
void beep_tick(void);

#endif
