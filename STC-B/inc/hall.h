/*
 * hall.h - 霍尔传感器 (SDCC)
 *
 * 霍尔传感器输入: P2.5 (active LOW / 需上板确认引脚)
 * 返回: HALL_AWAY=0, HALL_CLOSE=1
 */
#ifndef HALL_H
#define HALL_H

#define HALL_AWAY    0
#define HALL_CLOSE  1

void hall_init(void);
unsigned char hall_read(void);

#endif
