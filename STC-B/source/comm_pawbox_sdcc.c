/*
 * comm_pawbox_sdcc.c - paw_box 协议帧解析/发送 (SDCC 兼容)
 *
 * 跟 paw_box main.c 的 frame state machine 一致.
 * 这里只做 PING → PONG 一个命令, 验证 485 通信.
 */

#include "stc15_sdcc.h"
#include "bsp485_sdcc.h"
#include "protocol_sdcc.h"

#define FRAME_MAX  48

static unsigned char rx_frame[FRAME_MAX];
static unsigned char tx_frame[FRAME_MAX];
static unsigned char f_pos, f_state;
static unsigned char f_addr, f_cmd, f_plen;

unsigned char crc8_update(unsigned char crc, const unsigned char *d, unsigned char n)
{
    unsigned char i;
    while (n--)
    {
        crc ^= *d++;
        for (i = 0; i < 8; i++)
            crc = (crc & 0x80) ? (unsigned char)((crc << 1) ^ 0x07) : (unsigned char)(crc << 1);
    }
    return crc;
}

unsigned char crc8(const unsigned char *d, unsigned char n)
{
    return crc8_update(0, d, n);
}

static void frame_reset(void)
{
    f_state = 0;
    f_pos = 0;
}

/* 应答: CMD | RESP_FLAG + payload + CRC8 */
static void resp_send(unsigned char cmd, const unsigned char *payload, unsigned char len)
{
    unsigned char i, k = 0;

    tx_frame[k++] = PREAMBLE0;
    tx_frame[k++] = PREAMBLE1;
    tx_frame[k++] = SLAVE_ADDR;
    tx_frame[k++] = cmd | RESP_FLAG;
    tx_frame[k++] = len;
    for (i = 0; i < len; i++) tx_frame[k++] = payload[i];
    tx_frame[k++] = crc8(&tx_frame[2], (unsigned char)(3 + len));

    uart2_send_buf(tx_frame, k);
}

/* PING → PONG (空 payload) */
static void dispatch_ping(void)
{
    resp_send(CMD_PING, (const unsigned char *)0, 0);
}

/* 帧状态机: 在 main loop 里跑, 每收到 1 字节推进一步 */
void comm_poll(void)
{
    unsigned char b;

    while (rx_get(&b))
    {
        switch (f_state)
        {
        case 0: if (b == PREAMBLE0) f_state = 1; break;
        case 1: if (b == PREAMBLE1) f_state = 2; else frame_reset(); break;
        case 2: f_addr = b; f_state = 3; break;
        case 3: f_cmd = b; f_state = 4; break;
        case 4:
            if (b > FRAME_MAX) { frame_reset(); break; }
            f_plen = b; f_pos = 0;
            f_state = (f_plen == 0) ? 5 : 6;
            break;
        case 6:
            rx_frame[f_pos++] = b;
            if (f_pos >= f_plen) f_state = 5;
            break;
        case 5:
        {
            unsigned char h[3];
            h[0] = f_addr; h[1] = f_cmd; h[2] = f_plen;
            if (b == crc8_update(crc8(h, 3), rx_frame, f_plen) &&
                (f_addr == SLAVE_ADDR || f_addr == BROADCAST_ADDR))
            {
                if (f_addr == SLAVE_ADDR && (f_cmd & RESP_FLAG) == 0)
                {
                    /* 收到主站命令, 应答 */
                    if (f_cmd == CMD_PING) dispatch_ping();
                }
            }
            frame_reset();
            break;
        }
        default:
            frame_reset();
            break;
        }
    }
}

void comm_init(void)
{
    frame_reset();
    uart2_init();
}
