/************************************************************
 * 桌上宠物 - 串口通信模块 (comm.h)
 * STC-B ↔ ESP32-S3 通过 UART2(EXT) 通信
 * 协议版本: V1.0
 ************************************************************/

#ifndef _comm_H_
#define _comm_H_

/* ==================== 帧格式定义 ==================== */
#define FRAME_HEAD      0xAA    /* 帧头 */
#define FRAME_MAX_DATA  16      /* 最大数据载荷 */
#define FRAME_OVERHEAD  4       /* HEAD + CMD + LEN + CHK */

/* ==================== 下行命令 (ESP32 → STC-B) ==================== */
#define CMD_SET_EXPRESSION   0x01   /* 设置表情 [expr_id] */
#define CMD_SET_CUSTOM_FACE  0x02   /* 自定义段码 [s0..s7] */
#define CMD_SET_LED          0x03   /* 设置LED [led_byte] */
#define CMD_SET_BUZZER       0x04   /* 蜂鸣器 [freq_H, freq_L, time_H, time_L] */
#define CMD_SET_MOTOR        0x05   /* 步进电机 [speed, steps_H, steps_L] */
#define CMD_PLAY_SOUND       0x06   /* 播放音效 [sound_id] */
#define CMD_SET_ALL          0x07   /* 一次设置 [expr, led, sound] */
#define CMD_QUERY_SENSOR     0x10   /* 查询传感器 */
#define CMD_QUERY_TIME       0x11   /* 查询时间 */
#define CMD_SYS_PING         0xF0   /* 心跳 */
#define CMD_SYS_RESET        0xF1   /* 重置 */

/* ==================== 上行命令 (STC-B → ESP32) ==================== */
#define CMD_SENSOR_REPORT    0x20   /* 传感器上报 */
#define CMD_TIME_REPORT      0x21   /* 时间上报 */
#define CMD_EVENT_REPORT     0x22   /* 事件上报 */
#define CMD_ACK_OK           0xE0   /* 成功确认 */
#define CMD_ACK_FAIL         0xE1   /* 失败确认 */
#define CMD_SYS_PONG         0xF0   /* 心跳回应 */

/* ==================== 表情ID ==================== */
#define EXPR_SMILE      0x00    /* 笑脸 ◠‿◠ */
#define EXPR_LAUGH      0x01    /* 大笑 ^o^ */
#define EXPR_SAD        0x02    /* 难过 T_T */
#define EXPR_SLEEPY     0x03    /* 困倦 -.- */
#define EXPR_ANGRY      0x04    /* 生气 >_< */
#define EXPR_SURPRISE   0x05    /* 惊讶 O_O */
#define EXPR_LOVE       0x06    /* 爱心 <3 */
#define EXPR_EAT        0x07    /* 吃饭 nom */
#define EXPR_SICK       0x08    /* 生病 x_x */
#define EXPR_SLEEP      0x09    /* 睡觉 zZz */
#define EXPR_HELLO      0x0A    /* 问好 Hi! */
#define EXPR_PLAY       0x0B    /* 玩耍 ~_~ */
#define EXPR_COUNT      0x0C    /* 预设表情总数 */

/* ==================== 音效ID ==================== */
#define SOUND_SHORT     0x01    /* 短促嘀 */
#define SOUND_HAPPY     0x02    /* 开心双音 */
#define SOUND_SAD       0x03    /* 低落下降 */
#define SOUND_ALARM     0x04    /* 警告急促 */
#define SOUND_EAT       0x05    /* 吃东西 */
#define SOUND_SLEEP     0x06    /* 催眠低音 */
#define SOUND_WAKE      0x07    /* 起床音 */
#define SOUND_LOVE      0x08    /* 心声 */

/* ==================== 事件类型 ==================== */
#define EVENT_KEY       0x01    /* 按键事件 */
#define EVENT_NAV       0x02    /* 导航按键 */
#define EVENT_VIB       0x03    /* 振动 */
#define EVENT_HALL      0x04    /* 霍尔 */

/* ==================== 接收帧结构 ==================== */
typedef struct {
    unsigned char cmd;              /* 命令码 */
    unsigned char len;              /* 数据长度 */
    unsigned char data[FRAME_MAX_DATA]; /* 数据 */
} CommFrame;

/* ==================== 命令处理回调 ==================== */
typedef void (*CommCmdHandler)(CommFrame *frame);

/* ==================== 公共函数 ==================== */
extern void CommInit(unsigned long baud);
extern void CommSendFrame(unsigned char cmd, unsigned char *data, unsigned char len);
extern void CommSetCmdHandler(CommCmdHandler handler);

/* 便捷发送函数 */
extern void CommSendAck(unsigned char orig_cmd);
extern void CommSendNack(unsigned char orig_cmd, unsigned char error);
extern void CommSendSensorReport(unsigned int temp, unsigned int light,
                                  unsigned char buttons, unsigned char flags);
extern void CommSendTimeReport(unsigned char year, unsigned char month,
                                unsigned char day, unsigned char weekday,
                                unsigned char hour, unsigned char minute,
                                unsigned char second);
extern void CommSendEvent(unsigned char event_type, unsigned char event_data);

#endif
