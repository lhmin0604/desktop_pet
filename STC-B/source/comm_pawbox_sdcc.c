/*
 * comm_pawbox_sdcc.c - paw_box 协议帧解析/发送 (SDCC 兼容)
 *
 * 协议: [0xAA][0x55][ADDR][CMD][LEN][DATA][CRC8]
 *   - CRC8: 多项式 0x07, 初值 0x00, 范围 ADDR+CMD+LEN+DATA
 *
 * 物理: bsp485_sdcc.c 提供 uart2_init / uart2_send_buf / rx_get / uart2_isr
 *       走 STC-B 板上 MAX485 (P3.6/P3.7, T2 波特率)
 *
 * 接口设计: 避免 SDCC 8051 跨 TU 函数指针 storage class 不匹配的问题.
 *   - 收到完整主站命令后, 设置 volatile 全局变量:
 *       comm_cmd_ready = 1, comm_last_cmd = cmd, comm_last_len = len
 *   - main loop 里检查 comm_cmd_ready, 处理后清零
 *   - 载荷通过 comm_rx_payload[] 读取
 */

#include "stc15_sdcc.h"
#include "bsp485_sdcc.h"
#include "protocol_sdcc.h"

/* ============ 接收状态机 ============ */
#define ST_WAIT_P0    0
#define ST_WAIT_P1    1
#define ST_WAIT_ADDR  2
#define ST_WAIT_CMD   3
#define ST_WAIT_LEN   4
#define ST_WAIT_DATA  5
#define ST_WAIT_CRC   6

/* 走 BSS (BSS 清零工作正常). 之前加的 = 0 初始化反而走 XINIT, 但
 * SDCC 链接器 IHX 输出有 XINIT 偏移 bug, 会读到错误的 XDATA 内容. */
static unsigned char f_state;
static unsigned char f_addr;
static unsigned char f_cmd;
static unsigned char f_plen;
static unsigned char f_pos;

/* ============ 公开: 主站命令通知 (volatile, main loop 轮询) ============ */
volatile unsigned char comm_cmd_ready = 0;
volatile unsigned char comm_last_cmd = 0;
volatile unsigned char comm_last_len = 0;
volatile unsigned int  comm_rx_byte_count = 0;   /* 485 接收字节数 (debug) */
volatile unsigned int  comm_tx_byte_count = 0;   /* 485 发送字节数 (debug) */
unsigned char comm_rx_payload[FRAME_MAX_DATA];

/* ============ 发送缓冲 ============ */
static unsigned char f_txbuf[FRAME_MAX_DATA + FRAME_OVERHEAD];

/* ============ CRC8 ============ */
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

/* ============ 内部: 状态机复位 ============ */
static void frame_reset(void)
{
    f_state = ST_WAIT_P0;
    f_pos = 0;
}

/* ============ 公开: 发送 1 帧 (应答) ============ */
/* frame: 0xAA 0x55 ADDR (CMD|RESP_FLAG) LEN [DATA] CRC8 */
void comm_resp_send(unsigned char cmd, const unsigned char *payload, unsigned char len)
{
    unsigned char i, k = 0;

    if (len > FRAME_MAX_DATA) len = FRAME_MAX_DATA;

    f_txbuf[k++] = PREAMBLE0;
    f_txbuf[k++] = PREAMBLE1;
    f_txbuf[k++] = SLAVE_ADDR;
    f_txbuf[k++] = (unsigned char)(cmd | RESP_FLAG);
    f_txbuf[k++] = len;
    for (i = 0; i < len; i++) f_txbuf[k++] = payload[i];
    f_txbuf[k++] = crc8(&f_txbuf[2], (unsigned char)(3 + len));

    comm_tx_byte_count += k;
    uart2_send_buf(f_txbuf, k);
}

/* ============ 主动上报: 传感器数据 ============ */
/* ESP32 端 processFrame 期望: payload[0]=temp, [1]=light_L, [2]=light_H */
void comm_send_sensor(unsigned int temp, unsigned int light)
{
    unsigned char payload[3];
    payload[0] = (unsigned char)(temp & 0xFF);
    payload[1] = (unsigned char)(light & 0xFF);
    payload[2] = (unsigned char)((light >> 8) & 0xFF);
    comm_resp_send(CMD_GET_TEMP, payload, 3);
}

/* ============ 主动上报: 事件 ============ */
void comm_send_event(unsigned char event_type, unsigned char event_data)
{
    unsigned char payload[2];
    payload[0] = event_type;
    payload[1] = event_data;
    comm_resp_send(CMD_EVENT, payload, 2);
}

/* ============ 接收状态机 (在 main loop 里跑) ============ */
void comm_poll(void)
{
    unsigned char b;

    while (rx_get(&b))
    {
        comm_rx_byte_count++;
        switch (f_state)
        {
        case ST_WAIT_P0:
            if (b == PREAMBLE0) f_state = ST_WAIT_P1;
            break;

        case ST_WAIT_P1:
            if (b == PREAMBLE1) f_state = ST_WAIT_ADDR;
            else if (b != PREAMBLE0) f_state = ST_WAIT_P0;   /* 噪点重置 */
            /* b==PREAMBLE0 保持等下一个 P1 */
            break;

        case ST_WAIT_ADDR:
            f_addr = b;
            f_state = ST_WAIT_CMD;
            break;

        case ST_WAIT_CMD:
            f_cmd = b;
            f_state = ST_WAIT_LEN;
            break;

        case ST_WAIT_LEN:
            if (b > FRAME_MAX_DATA) { frame_reset(); break; }
            f_plen = b;
            f_pos = 0;
            f_state = (f_plen == 0) ? ST_WAIT_CRC : ST_WAIT_DATA;
            break;

        case ST_WAIT_DATA:
            comm_rx_payload[f_pos++] = b;
            if (f_pos >= f_plen) f_state = ST_WAIT_CRC;
            break;

        case ST_WAIT_CRC:
        {
            unsigned char h[3];
            h[0] = f_addr; h[1] = f_cmd; h[2] = f_plen;
            if (b == crc8_update(crc8(h, 3), comm_rx_payload, f_plen) &&
                (f_addr == SLAVE_ADDR || f_addr == BROADCAST_ADDR))
            {
                /* 只处理主站命令 (CMD & 0x80 == 0) */
                if (f_addr == SLAVE_ADDR && (f_cmd & RESP_FLAG) == 0)
                {
                    comm_last_cmd = f_cmd;
                    comm_last_len = f_plen;
                    comm_cmd_ready = 1;
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

/* ============ 初始化 ============ */
void comm_init(void)
{
    frame_reset();
    comm_cmd_ready = 0;
    comm_last_cmd = 0;
    comm_last_len = 0;
    uart2_init();
}
