/*
 * vib.h - 振动电机 + 振动传感器 (SDCC)
 *
 * 振动电机输出: P2.4 (通过 ULN2003)
 * 振动传感器输入: P3.5 (active LOW, 需上板确认引脚)
 */
#ifndef VIB_H
#define VIB_H

#define VIB_QUIET   0
#define VIB_QUAKE   1

void vib_init(void);
void vib_motor_on(void);
void vib_motor_off(void);
unsigned char vib_sensor_read(void);

#endif
