/************************************************************
 * 桌上宠物 - 串口通信模块实现 (comm.c)
 * 基于 STC-B BSP 的 UART2 驱动
 ************************************************************/

#include "STC15F2K60S2.H"
#include "sys.H"
#include "uart2.h"
#include "comm.h"

/* ==================== 内部变量 ==================== */
static unsigned char rxd_buf[FRAME_MAX_DATA + FRAME_OVERHEAD];  /* 接收缓冲区 */
static CommCmdHandler cmd_handler = 0;   /* 用户注册的命令处理回调 */

/* 接收状态机 */
#define RX_WAIT_HEAD  0
#define RX_WAIT_CMD   1
#define RX_WAIT_LEN   2
#define RX_WAIT_DATA  3
#define RX_WAIT_CHK   4

static unsigned char rx_state = RX_WAIT_HEAD;
static unsigned char rx_cmd;
static unsigned char rx_len;
static unsigned char rx_data_idx;
static unsigned char rx_chk;
static unsigned char rx_data_buf[FRAME_MAX_DATA];

/* 发送缓冲区 */
static unsigned char txd_buf[FRAME_MAX_DATA + FRAME_OVERHEAD];

/* ==================== 内部函数 ==================== */

/* 计算校验和: HEAD ^ CMD ^ LEN ^ DATA[0..n] */
static unsigned char CalcChecksum(unsigned char cmd, unsigned char len,
                                   unsigned char *data)
{
    unsigned char chk = FRAME_HEAD ^ cmd ^ len;
    unsigned char i;
    for(i = 0; i < len; i++)
        chk ^= data[i];
    return chk;
}

/* 处理接收到的完整帧 */
static void ProcessFrame(void)
{
    CommFrame frame;
    unsigned char i;

    frame.cmd = rx_cmd;
    frame.len = rx_len;
    for(i = 0; i < rx_len && i < FRAME_MAX_DATA; i++)
        frame.data[i] = rx_data_buf[i];

    /* 调用用户注册的处理函数 */
    if(cmd_handler != 0)
        cmd_handler(&frame);
}

/* UART2 接收回调（BSP 事件驱动） */
static void OnUart2Rxd(void)
{
    unsigned char i, n;
    unsigned char byte;

    n = GetUart2RxNum();
    for(i = 0; i < n; i++)
    {
        byte = rxd_buf[i];  /* 从接收缓冲区读取 */

        switch(rx_state)
        {
            case RX_WAIT_HEAD:
                if(byte == FRAME_HEAD)
                {
                    rx_chk = FRAME_HEAD;
                    rx_state = RX_WAIT_CMD;
                }
                break;

            case RX_WAIT_CMD:
                rx_cmd = byte;
                rx_chk ^= byte;
                rx_state = RX_WAIT_LEN;
                break;

            case RX_WAIT_LEN:
                rx_len = byte;
                rx_chk ^= byte;
                if(rx_len > FRAME_MAX_DATA)
                {
                    /* 长度超限，丢弃帧 */
                    rx_state = RX_WAIT_HEAD;
                }
                else if(rx_len == 0)
                {
                    rx_state = RX_WAIT_CHK;
                }
                else
                {
                    rx_data_idx = 0;
                    rx_state = RX_WAIT_DATA;
                }
                break;

            case RX_WAIT_DATA:
                rx_data_buf[rx_data_idx++] = byte;
                rx_chk ^= byte;
                if(rx_data_idx >= rx_len)
                    rx_state = RX_WAIT_CHK;
                break;

            case RX_WAIT_CHK:
                if(byte == rx_chk)
                {
                    /* 校验通过，处理帧 */
                    ProcessFrame();
                }
                /* 不论校验是否通过，都回到等待帧头 */
                rx_state = RX_WAIT_HEAD;
                break;

            default:
                rx_state = RX_WAIT_HEAD;
                break;
        }
    }
}

/* ==================== 公共函数 ==================== */

void CommInit(unsigned long baud)
{
    /* 初始化 UART2，使用 EXT 口 */
    Uart2Init(baud);

    /* 设置接收: 单字节接收（Nmax=1），不使用帧头匹配，
       由我们自己的状态机来解析帧 */
    SetUart2Rxd(rxd_buf, 1, 0, 0);

    /* 注册 UART2 接收事件回调 */
    SetEventCallBack(enumEventUart2Rxd, OnUart2Rxd);

    /* 初始化接收状态机 */
    rx_state = RX_WAIT_HEAD;
}

void CommSendFrame(unsigned char cmd, unsigned char *data, unsigned char len)
{
    unsigned char i;
    unsigned char idx = 0;
    unsigned char chk;

    if(len > FRAME_MAX_DATA)
        len = FRAME_MAX_DATA;

    /* 构建帧 */
    txd_buf[idx++] = FRAME_HEAD;
    txd_buf[idx++] = cmd;
    txd_buf[idx++] = len;

    chk = FRAME_HEAD ^ cmd ^ len;
    for(i = 0; i < len; i++)
    {
        txd_buf[idx++] = data[i];
        chk ^= data[i];
    }
    txd_buf[idx++] = chk;

    /* 发送 */
    Uart2Print(txd_buf, idx);
}

void CommSetCmdHandler(CommCmdHandler handler)
{
    cmd_handler = handler;
}

/* ---- 便捷发送函数 ---- */

void CommSendAck(unsigned char orig_cmd)
{
    CommSendFrame(CMD_ACK_OK, &orig_cmd, 1);
}

void CommSendNack(unsigned char orig_cmd, unsigned char error)
{
    unsigned char data[2];
    data[0] = orig_cmd;
    data[1] = error;
    CommSendFrame(CMD_ACK_FAIL, data, 2);
}

void CommSendSensorReport(unsigned int temp, unsigned int light,
                           unsigned char buttons, unsigned char flags)
{
    unsigned char data[6];
    data[0] = (unsigned char)(temp >> 8);
    data[1] = (unsigned char)(temp & 0xFF);
    data[2] = (unsigned char)(light >> 8);
    data[3] = (unsigned char)(light & 0xFF);
    data[4] = buttons;
    data[5] = flags;
    CommSendFrame(CMD_SENSOR_REPORT, data, 6);
}

void CommSendTimeReport(unsigned char year, unsigned char month,
                         unsigned char day, unsigned char weekday,
                         unsigned char hour, unsigned char minute,
                         unsigned char second)
{
    unsigned char data[7];
    data[0] = year;
    data[1] = month;
    data[2] = day;
    data[3] = weekday;
    data[4] = hour;
    data[5] = minute;
    data[6] = second;
    CommSendFrame(CMD_TIME_REPORT, data, 7);
}

void CommSendEvent(unsigned char event_type, unsigned char event_data)
{
    unsigned char data[2];
    data[0] = event_type;
    data[1] = event_data;
    CommSendFrame(CMD_EVENT_REPORT, data, 2);
}
