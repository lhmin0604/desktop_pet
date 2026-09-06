/*
 * comm_esp32.c - 通信协议帧解析/发送 (SDCC)
 *
 * 帧格式 (跟 ESP32 PetProtocol 一致):
 *   [0xAA] [CMD] [LEN] [DATA[0..LEN-1]] [CHK]
 *   CHK = HEAD ^ CMD ^ LEN ^ DATA[0..LEN-1]
 *
 * 在 main loop 里调用 comm_poll(), 从 UART2 ring buffer 读字节推进状态机.
 */
#include "stc15_sdcc.h"
#include "uart2_ext.h"
#include "protocol_esp32.h"

typedef void (*CommCmdHandler)(CommFrame *frame);

static CommFrame rx_frame;
static unsigned char f_state;
static unsigned char f_cmd;
static unsigned char f_len;
static unsigned char f_pos;
static unsigned char f_chk;

static CommCmdHandler cmd_handler = 0;

/* 发送缓冲区 */
static unsigned char tx_buf[FRAME_MAX_DATA + FRAME_OVERHEAD];

/* ==================== 校验 ==================== */
static unsigned char calc_chk(unsigned char cmd, unsigned char len,
                               const unsigned char *data)
{
    unsigned char chk = FRAME_HEAD ^ cmd ^ len;
    unsigned char i;
    for (i = 0; i < len; i++)
        chk ^= data[i];
    return chk;
}

/* ==================== 帧处理 ==================== */
static void process_frame(void)
{
    if (cmd_handler)
        cmd_handler(&rx_frame);
}

/* ==================== 发送 ==================== */
void comm_send_frame(unsigned char cmd, const unsigned char *data,
                      unsigned char len)
{
    unsigned char i, idx = 0;

    if (len > FRAME_MAX_DATA) len = FRAME_MAX_DATA;

    tx_buf[idx++] = FRAME_HEAD;
    tx_buf[idx++] = cmd;
    tx_buf[idx++] = len;
    for (i = 0; i < len; i++)
        tx_buf[idx++] = data[i];
    tx_buf[idx++] = calc_chk(cmd, len, data);

    uart2_send_buf(tx_buf, idx);
}

void comm_send_ack(unsigned char orig_cmd)
{
    comm_send_frame(CMD_ACK_OK, &orig_cmd, 1);
}

void comm_send_nack(unsigned char orig_cmd, unsigned char error)
{
    unsigned char data[2];
    data[0] = orig_cmd;
    data[1] = error;
    comm_send_frame(CMD_ACK_FAIL, data, 2);
}

void comm_send_sensor_report(unsigned int temp, unsigned int light,
                              unsigned char buttons, unsigned char flags)
{
    unsigned char data[6];
    data[0] = (unsigned char)(temp >> 8);
    data[1] = (unsigned char)(temp & 0xFF);
    data[2] = (unsigned char)(light >> 8);
    data[3] = (unsigned char)(light & 0xFF);
    data[4] = buttons;
    data[5] = flags;
    comm_send_frame(CMD_SENSOR_REPORT, data, 6);
}

void comm_send_event(unsigned char event_type, unsigned char event_data)
{
    unsigned char data[2];
    data[0] = event_type;
    data[1] = event_data;
    comm_send_frame(CMD_EVENT_REPORT, data, 2);
}

/* ==================== 初始化 ==================== */
void comm_init(void)
{
    f_state = 0;
    f_pos = 0;
    uart2_init();
}

void comm_set_handler(CommCmdHandler handler)
{
    cmd_handler = handler;
}

/* ==================== 接收状态机 ==================== */
void comm_poll(void)
{
    unsigned char b;

    while (uart2_rx_available())
    {
        b = uart2_rx_get();

        switch (f_state)
        {
        case 0:   /* 等待帧头 */
            if (b == FRAME_HEAD)
            {
                f_chk = FRAME_HEAD;
                f_state = 1;
            }
            break;

        case 1:   /* 等待 CMD */
            f_cmd = b;
            f_chk ^= b;
            f_state = 2;
            break;

        case 2:   /* 等待 LEN */
            f_len = b;
            f_chk ^= b;
            if (f_len > FRAME_MAX_DATA)
            {
                f_state = 0;   /* 长度超限, 丢弃 */
            }
            else if (f_len == 0)
            {
                f_state = 4;   /* 无数据, 直接等 CHK */
            }
            else
            {
                f_pos = 0;
                f_state = 3;
            }
            break;

        case 3:   /* 接收 DATA */
            rx_frame.data[f_pos++] = b;
            f_chk ^= b;
            if (f_pos >= f_len)
                f_state = 4;
            break;

        case 4:   /* 等待 CHK */
            if (b == f_chk)
            {
                rx_frame.cmd = f_cmd;
                rx_frame.len = f_len;
                process_frame();
            }
            f_state = 0;
            break;

        default:
            f_state = 0;
            break;
        }
    }
}
