/*
 * main_pawbox_sdcc.c - 桌上宠物 STC-B 主程序 (paw_box 协议, SDCC 版)
 *
 * Phase 1: 只验证 485 通信
 *   ESP32 发送: AA 55 01 00 00 [CRC8]    (PING)
 *   STC-B 应答: AA 55 01 80 00 [CRC8]    (PONG)
 *
 * 硬件:
 *   - STC-B 板上 MAX485 (TXD2=P3.7 RXD2=P3.6, RS485_DE=P3.7)
 *   - 3 针端子 A, B, GND 接到 ESP32 端 485 模块
 */

#include "stc15_sdcc.h"
#include "bsp485_sdcc.h"
#include "protocol_sdcc.h"

extern void comm_init(void);
extern void comm_poll(void);

int main(void)
{
    comm_init();        /* 初始化 485 + 帧状态机 */
    /* EA 已经在 uart2_init 里开了 */

    while (1)
    {
        comm_poll();    /* 处理接收字节 */
    }

    return 0;
}
