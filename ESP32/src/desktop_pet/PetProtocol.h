/************************************************************
 * 桌上宠物 - 通信协议处理器 (PetProtocol.h)
 * 处理 ESP32 ↔ STC-B 的串口通信 (paw_box 协议 + RS485 DE 控制)
 *
 * 协议格式 (paw_box 兼容):
 *   [0xAA] [0x55] [ADDR] [CMD] [LEN] [DATA[0..LEN-1]] [CRC8]
 *   CRC8 多项式 0x07, 初值 0x00, 范围 ADDR..DATA
 *
 * 硬件:
 *   Serial2 (TX=GPIO43, RX=GPIO44)
 *   RS485 DE = GPIO38 (Pmod 左上)
 ************************************************************/

#ifndef PET_PROTOCOL_H
#define PET_PROTOCOL_H

#include <Arduino.h>

/* ============ 帧格式常量 (paw_box) ============ */
#define FRAME_PREAMBLE0   0xAA
#define FRAME_PREAMBLE1   0x55
#define SLAVE_ADDR        0x01    /* STC-B 从机地址 */
#define FRAME_MAX_DATA    48      /* paw_box FRAME_MAX = 48 */
#define FRAME_OVERHEAD    7       /* 2*PREAMBLE + ADDR + CMD + LEN + CRC8 */

/* ============ 命令码 (paw_box 兼容) ============ */
#define PB_CMD_PING        0x00
#define PB_CMD_GET_TEMP    0x01
#define PB_CMD_GET_LIGHT   0x02
#define PB_CMD_IR_NEC      0x10
#define PB_CMD_IR_RAW      0x11
#define PB_CMD_AC_GREE     0x12
#define PB_CMD_LED_SET     0x20
#define PB_CMD_SET_EXPR    0x21    /* 扩展: 设置表情 (我们桌面宠物用) */
#define PB_RESP_FLAG       0x80

/* 高层 API 命令 (跟原 PetProtocol 保持兼容) */
#define CMD_SET_EXPRESSION 0x01    /* 发送时映射到 PB_CMD_SET_EXPR */
#define CMD_SET_LED        0x03    /* 发送时映射到 PB_CMD_LED_SET */
#define CMD_QUERY_SENSOR   0x10
#define CMD_QUERY_TIME     0x11
#define CMD_SYS_PING       0xF0    /* 内部 PING, 映射到 PB_CMD_PING */
#define CMD_SYS_RESET      0xF1

/* 上行命令 (STC-B → ESP32) — 用 paw_box 的 RESP_FLAG 形式 */
#define CMD_SENSOR_REPORT  (PB_CMD_GET_TEMP | PB_RESP_FLAG)   /* 0x81 */
#define CMD_EVENT_REPORT   0xA0
#define CMD_ACK_OK         0xE0
#define CMD_ACK_FAIL       0xE1
#define CMD_SYS_PONG       (PB_CMD_PING | PB_RESP_FLAG)        /* 0x80 */

/* ============ 表情ID ============ */
#define EXPR_SMILE      0x00
#define EXPR_LAUGH      0x01
#define EXPR_SAD        0x02
#define EXPR_SLEEPY     0x03
#define EXPR_ANGRY      0x04
#define EXPR_SURPRISE   0x05
#define EXPR_LOVE       0x06
#define EXPR_EAT        0x07
#define EXPR_SICK       0x08
#define EXPR_SLEEP      0x09
#define EXPR_HELLO      0x0A
#define EXPR_PLAY       0x0B

/* ============ 事件类型 ============ */
#define EVENT_KEY       0x01
#define EVENT_NAV       0x02
#define EVENT_VIB       0x03
#define EVENT_HALL      0x04

/* ============ 回调类型 ============ */
typedef void (*SensorCallback)(uint16_t temp, uint16_t light,
                                uint8_t buttons, uint8_t flags);
typedef void (*EventCallback)(uint8_t type, uint8_t data);
typedef void (*AckCallback)(uint8_t cmd, bool success);
typedef void (*PongCallback)();

class PetProtocol {
public:
    PetProtocol();

    void begin(HardwareSerial* serial, int8_t de_pin = -1);
    void process();

    /* 发送函数 */
    void sendExpression(uint8_t expr_id);
    void sendCustomFace(uint8_t seg[8]);
    void sendLed(uint8_t led_val);
    void sendBuzzer(uint16_t freq, uint16_t time_10ms);
    void sendMotor(uint8_t speed, int16_t steps);
    void sendSound(uint8_t sound_id);
    void sendAll(uint8_t expr, uint8_t led, uint8_t sound);
    void sendQuerySensor();
    void sendQueryTime();
    void sendPing();
    void sendReset();

    /* 回调 */
    void onSensorReport(SensorCallback cb);
    void onEvent(EventCallback cb);
    void onAck(AckCallback cb);
    void onPong(PongCallback cb);

private:
    HardwareSerial* _serial;
    int8_t _de_pin;  /* RS485 DE 控制脚, -1 = 不控制 */

    /* 接收状态机 */
    enum RxState {
        RX_WAIT_PREAMBLE0,
        RX_WAIT_PREAMBLE1,
        RX_WAIT_ADDR,
        RX_WAIT_CMD,
        RX_WAIT_LEN,
        RX_WAIT_DATA,
        RX_WAIT_CRC
    };

    RxState _rx_state;
    uint8_t _rx_addr;
    uint8_t _rx_cmd;
    uint8_t _rx_len;
    uint8_t _rx_data_idx;
    uint8_t _rx_crc;
    uint8_t _rx_data[FRAME_MAX_DATA];

    SensorCallback _cb_sensor;
    EventCallback  _cb_event;
    AckCallback    _cb_ack;
    PongCallback   _cb_pong;

    /* 内部 */
    void sendFrame(uint8_t pb_cmd, const uint8_t* data, uint8_t len);
    void processFrame();
    static uint8_t crc8(const uint8_t* d, uint8_t n);
    static uint8_t crc8_update(uint8_t crc, const uint8_t* d, uint8_t n);
    void setDE(bool tx_mode);
};

#endif
