/*
 * keys.h - 按键驱动 (SDCC)
 *
 * K1=P3.2, K2=P3.3, K3=P1.7 (active LOW, 内部上拉)
 * 10ms 消抖, 检测 press / release
 */
#ifndef KEYS_H
#define KEYS_H

#define KEY_PRESS    1
#define KEY_RELEASE  2
#define KEY_NONE     0

#define KEY_ID_K1   0
#define KEY_ID_K2   1
#define KEY_ID_K3   2

void keys_init(void);
unsigned char keys_scan(void);   /* 返回按下的 key_id + 事件, 0=无事件 */
unsigned char keys_get_event(unsigned char key_id);

#endif
