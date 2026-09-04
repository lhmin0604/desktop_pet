/************************************************************
 * 桌上宠物 - 通信协议处理器 (PetProtocol.h)
 * 处理 ESP32 ↔ STC-B 的串口通信
 ************************************************************/

#ifndef PET_PROTOCOL_H
#define PET_PROTOCOL_H

#include <Arduino.h>

/* 帧格式常量 */
#define FRAME_HEAD        0xAA
#define FRAME_MAX_DATA    16
#define FRAME_OVERHEAD    4    /* HEAD + CMD + LEN + CHK */

/* 下行命令 (ESP32 → STC-B) */
#define CMD_SET_EXPRESSION   0x01
#define CMD_SET_CUSTOM_FACE  0x02
#define CMD_SET_LED          0x03
#define CMD_SET_BUZZER       0x04
#define CMD_SET_MOTOR        0x05
#define CMD_PLAY_SOUND       0x06
#define CMD_SET_ALL          0x07
#define CMD_QUERY_SENSOR     0x10
#define CMD_QUERY_TIME       0x11
#define CMD_SYS_PING         0xF0
#define CMD_SYS_RESET        0xF1

/* 上行命令 (STC-B → ESP32) */
#define CMD_SENSOR_REPORT    0x20
#define CMD_TIME_REPORT      0x21
#define CMD_EVENT_REPORT     0x22
#define CMD_ACK_OK           0xE0
#define CMD_ACK_FAIL         0xE1
#define CMD_SYS_PONG         0xF0

/* 表情ID */
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

/* 音效ID */
#define SOUND_SHORT     0x01
#define SOUND_HAPPY     0x02
#define SOUND_SAD       0x03
#define SOUND_ALARM     0x04
#define SOUND_EAT       0x05
#define SOUND_SLEEP     0x06
#define SOUND_WAKE      0x07
#define SOUND_LOVE      0x08

/* 事件类型 */
#define EVENT_KEY       0x01
#define EVENT_NAV       0x02
#define EVENT_VIB       0x03
#define EVENT_HALL      0x04

/* 回调函数类型 */
typedef void (*SensorCallback)(uint16_t temp, uint16_t light,
                                uint8_t buttons, uint8_t flags);
typedef void (*EventCallback)(uint8_t type, uint8_t data);
typedef void (*AckCallback)(uint8_t cmd, bool success);
typedef void (*PongCallback)();

class PetProtocol {
public:
    PetProtocol();

    void begin(HardwareSerial* serial);

    /* 处理串口接收（在 loop 中反复调用） */
    void process();

    /* 下行发送函数 */
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

    /* 注册回调 */
    void onSensorReport(SensorCallback cb);
    void onEvent(EventCallback cb);
    void onAck(AckCallback cb);
    void onPong(PongCallback cb);

private:
    HardwareSerial* _serial;

    /* 接收状态机 */
    enum RxState {
        RX_WAIT_HEAD,
        RX_WAIT_CMD,
        RX_WAIT_LEN,
        RX_WAIT_DATA,
        RX_WAIT_CHK
    };

    RxState _rx_state;
    uint8_t _rx_cmd;
    uint8_t _rx_len;
    uint8_t _rx_data_idx;
    uint8_t _rx_chk;
    uint8_t _rx_data[FRAME_MAX_DATA];

    /* 回调函数指针 */
    SensorCallback _cb_sensor;
    EventCallback  _cb_event;
    AckCallback    _cb_ack;
    PongCallback   _cb_pong;

    /* 内部函数 */
    void sendFrame(uint8_t cmd, const uint8_t* data, uint8_t len);
    void processFrame();
    uint8_t calcChecksum(uint8_t cmd, uint8_t len, const uint8_t* data);
};

#endif
