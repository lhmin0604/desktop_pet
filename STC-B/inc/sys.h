/*
 * sys.h - 系统定时器 (SDCC)
 *
 * Timer0 产生 1ms 系统时基, 用于:
 *   - 数码管动态扫描 (每2ms切换一位)
 *   - 按键消抖 (10ms)
 *   - 传感器采样 (10ms)
 *   - 表情动画 (100ms)
 *   - 传感器上报 (500ms)
 */
#ifndef SYS_H
#define SYS_H

extern volatile unsigned int sys_tick_ms;
extern volatile unsigned char sys_flag_10ms;
extern volatile unsigned char sys_flag_100ms;
extern volatile unsigned char sys_flag_1000ms;

void sys_init(void);

#endif
