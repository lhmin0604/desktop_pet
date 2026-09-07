/*
 * keys.c - 按键驱动 (SDCC)
 *
 * K1=P3.2, K2=P3.3, K3=P1.7
 * 10ms 扫描消抖, 检测 press/release
 */
#include "stc15_sdcc.h"
#include "keys.h"

/* 走 BSS (BSS 清零工作正常). 之前加的 = {0} 反而走 XINIT, 触发 SDCC
 * 链接器的 XINIT 偏移 bug, 让按键状态机从随机值开始. */
static unsigned char key_raw[3];
static unsigned char key_stable[3];
static unsigned char key_prev[3];
static unsigned char key_event[3];

void keys_init(void)
{
    P3M0 &= ~0x0C;    /* P3.2, P3.3 quasi-bidirectional */
    P3M1 |= 0x0C;
    P3  |= 0x0C;      /* 内部上拉 */
    P1M0 &= ~0x80;    /* P1.7 quasi-bidirectional */
    P1M1 |= 0x80;
    P1  |= 0x80;
}

static unsigned char read_key(unsigned char id)
{
    switch (id)
    {
        case 0: return P32;
        case 1: return P33;
        case 2: return P17;
        default: return 1;
    }
}

/* 每 10ms 调用一次 */
unsigned char keys_scan(void)
{
    unsigned char i, ev;

    for (i = 0; i < 3; i++)
    {
        key_event[i] = KEY_NONE;

        /* 消抖: 连续两次读取相同才确认 */
        key_raw[i] = (key_raw[i] << 1) | read_key(i);
        if ((key_raw[i] & 0x03) == 0x03)
            key_stable[i] = 1;    /* 释放 (HIGH) */
        else if ((key_raw[i] & 0x03) == 0x00)
            key_stable[i] = 0;    /* 按下 (LOW) */

        /* 检测边沿 */
        if (key_stable[i] != key_prev[i])
        {
            if (key_stable[i] == 0)
                key_event[i] = KEY_PRESS;
            else
                key_event[i] = KEY_RELEASE;
            key_prev[i] = key_stable[i];
        }
    }

    /* 返回第一个有事件的 key */
    for (i = 0; i < 3; i++)
    {
        ev = key_event[i];
        if (ev != KEY_NONE)
            return (unsigned char)(i * 4 + ev);  /* 编码: id*4 + event */
    }
    return 0;
}

unsigned char keys_get_event(unsigned char key_id)
{
    unsigned char ev;
    if (key_id > 2) return KEY_NONE;
    ev = key_event[key_id];
    key_event[key_id] = KEY_NONE;
    return ev;
}
