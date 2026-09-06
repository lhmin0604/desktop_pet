/*
 * vib.c - 振动电机 + 振动传感器 (SDCC)
 */
#include "stc15_sdcc.h"
#include "vib.h"

void vib_init(void)
{
    P2M0 |= 0x10;    /* P2.4 push-pull (电机输出) */
    P2M1 &= ~0x10;
    P24 = 0;          /* 电机 off */

    P3M0 &= ~0x20;   /* P3.5 quasi-bidirectional (传感器输入) */
    P3M1 |= 0x20;
    P3 |= 0x20;       /* 内部上拉 */
}

void vib_motor_on(void)  { P24 = 1; }
void vib_motor_off(void) { P24 = 0; }

unsigned char vib_sensor_read(void)
{
    return (P35 == 0) ? VIB_QUAKE : VIB_QUIET;
}
