/*
 * hall.c - 霍尔传感器 (SDCC)
 */
#include "stc15_sdcc.h"
#include "hall.h"

void hall_init(void)
{
    P2M0 &= ~0x20;   /* P2.5 input */
    P2M1 |= 0x20;    /* quasi-bidirectional */
    P2 |= 0x20;       /* 内部上拉 */
}

unsigned char hall_read(void)
{
    return (P25 == 0) ? HALL_CLOSE : HALL_AWAY;
}
