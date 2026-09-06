/************************************************************
 * 桌上宠物 - 通信协议处理器实现 (PetProtocol.cpp)
 * paw_box 协议 + RS485 DE 控制
 ************************************************************/

#include "PetProtocol.h"

PetProtocol::PetProtocol()
    : _serial(nullptr),
      _de_pin(-1),
      _rx_state(RX_WAIT_PREAMBLE0),
      _cb_sensor(nullptr),
      _cb_event(nullptr),
      _cb_ack(nullptr),
      _cb_pong(nullptr) {
}

void PetProtocol::begin(HardwareSerial* serial, int8_t de_pin) {
    _serial = serial;
    _de_pin = de_pin;
    _rx_state = RX_WAIT_PREAMBLE0;

    if (_de_pin >= 0) {
        pinMode(_de_pin, OUTPUT);
        digitalWrite(_de_pin, LOW);  /* 默认接收态 */
    }
}

/* ==================== RS485 DE 控制 ==================== */
void PetProtocol::setDE(bool tx_mode) {
    if (_de_pin >= 0) {
        digitalWrite(_de_pin, tx_mode ? HIGH : LOW);
        if (tx_mode) {
            /* DE 高到数据发出: 等几 us 让 485 收发器稳定 */
            delayMicroseconds(50);
        } else {
            /* 发送完成: 等待最后停止位移出, 再切回接收 */
            delayMicroseconds(200);
        }
    }
}

/* ==================== CRC8 (polynomial 0x07) ==================== */
uint8_t PetProtocol::crc8_update(uint8_t crc, const uint8_t* d, uint8_t n) {
    while (n--) {
        crc ^= *d++;
        for (uint8_t i = 0; i < 8; i++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

uint8_t PetProtocol::crc8(const uint8_t* d, uint8_t n) {
    return crc8_update(0, d, n);
}

/* ==================== 接收处理 ==================== */

void PetProtocol::process() {
    while (_serial && _serial->available()) {
        uint8_t byte = _serial->read();

        switch (_rx_state) {
            case RX_WAIT_PREAMBLE0:
                if (byte == FRAME_PREAMBLE0) {
                    _rx_crc = 0;
                    _rx_state = RX_WAIT_PREAMBLE1;
                }
                break;

            case RX_WAIT_PREAMBLE1:
                if (byte == FRAME_PREAMBLE1) {
                    _rx_state = RX_WAIT_ADDR;
                } else if (byte == FRAME_PREAMBLE0) {
                    /* 保持等 PREAMBLE0 (一个 0xAA 重复) */
                } else {
                    _rx_state = RX_WAIT_PREAMBLE0;
                }
                break;

            case RX_WAIT_ADDR:
                _rx_addr = byte;
                _rx_crc = crc8_update(0, &byte, 1);
                _rx_state = RX_WAIT_CMD;
                break;

            case RX_WAIT_CMD:
                _rx_cmd = byte;
                _rx_crc = crc8_update(_rx_crc, &byte, 1);
                _rx_state = RX_WAIT_LEN;
                break;

            case RX_WAIT_LEN:
                _rx_len = byte;
                _rx_crc = crc8_update(_rx_crc, &byte, 1);
                if (_rx_len > FRAME_MAX_DATA) {
                    _rx_state = RX_WAIT_PREAMBLE0;
                } else if (_rx_len == 0) {
                    _rx_state = RX_WAIT_CRC;
                } else {
                    _rx_data_idx = 0;
                    _rx_state = RX_WAIT_DATA;
                }
                break;

            case RX_WAIT_DATA:
                _rx_data[_rx_data_idx++] = byte;
                _rx_crc = crc8_update(_rx_crc, &byte, 1);
                if (_rx_data_idx >= _rx_len) {
                    _rx_state = RX_WAIT_CRC;
                }
                break;

            case RX_WAIT_CRC:
                if (byte == _rx_crc && _rx_addr == SLAVE_ADDR) {
                    processFrame();
                }
                _rx_state = RX_WAIT_PREAMBLE0;
                break;
        }
    }
}

void PetProtocol::processFrame() {
    /* STC-B 应答的命令都带 RESP_FLAG */
    uint8_t cmd = _rx_cmd;
    bool is_resp = (cmd & PB_RESP_FLAG) != 0;
    uint8_t pb_cmd = cmd & 0x7F;  /* 去掉 RESP_FLAG */

    if (!is_resp) {
        /* 主站不会收到不带 RESP_FLAG 的命令 (除非有别的从机) */
        return;
    }

    switch (pb_cmd) {
        case PB_CMD_PING:
            /* PING 应答 → 心跳 */
            if (_cb_pong) _cb_pong();
            break;

        case PB_CMD_GET_TEMP:
            /* 温度/光照合并上报 (跟 paw_box 一样, payload[0]=temp, payload[1..2]=light) */
            if (_rx_len >= 3 && _cb_sensor) {
                uint16_t temp = _rx_data[0];
                uint16_t light = ((uint16_t)_rx_data[1]) | ((uint16_t)_rx_data[2] << 8);
                _cb_sensor(temp, light, 0, 0);
            }
            break;

        case PB_CMD_GET_LIGHT:
            if (_rx_len >= 2 && _cb_sensor) {
                uint16_t light = ((uint16_t)_rx_data[0]) | ((uint16_t)_rx_data[1] << 8);
                _cb_sensor(0, light, 0, 0);
            }
            break;

        case PB_CMD_LED_SET:
            if (_rx_len >= 1 && _cb_ack) _cb_ack(pb_cmd, _rx_data[0] == 0);
            break;

        case PB_CMD_SET_EXPR:
            if (_rx_len >= 1 && _cb_ack) _cb_ack(pb_cmd, _rx_data[0] == 0);
            break;

        default:
            Serial.printf("[协议] 未知应答: 0x%02X (pb=0x%02X)\n", cmd, pb_cmd);
            break;
    }
}

/* ==================== 发送函数 ==================== */

void PetProtocol::sendFrame(uint8_t pb_cmd, const uint8_t* data, uint8_t len) {
    if (!_serial) return;
    if (len > FRAME_MAX_DATA) len = FRAME_MAX_DATA;

    uint8_t frame[FRAME_MAX_DATA + FRAME_OVERHEAD];
    uint8_t idx = 0;

    frame[idx++] = FRAME_PREAMBLE0;
    frame[idx++] = FRAME_PREAMBLE1;
    frame[idx++] = SLAVE_ADDR;
    frame[idx++] = pb_cmd;
    frame[idx++] = len;

    for (uint8_t i = 0; i < len; i++) {
        frame[idx++] = data[i];
    }

    /* CRC8 over ADDR + CMD + LEN + DATA */
    frame[idx++] = crc8(&frame[2], (uint8_t)(3 + len));

    /* RS485 发送 */
    setDE(true);
    _serial->write(frame, idx);
    _serial->flush();
    setDE(false);
}

void PetProtocol::sendExpression(uint8_t expr_id) {
    uint8_t data[] = { expr_id };
    sendFrame(PB_CMD_SET_EXPR, data, 1);
}

void PetProtocol::sendCustomFace(uint8_t seg[8]) {
    sendFrame(PB_CMD_SET_EXPR, seg, 8);
}

void PetProtocol::sendLed(uint8_t led_val) {
    uint8_t data[] = { led_val };
    sendFrame(PB_CMD_LED_SET, data, 1);
}

void PetProtocol::sendBuzzer(uint16_t freq, uint16_t time_10ms) {
    /* paw_box 没单独的蜂鸣器命令, 用 LED_SET 模拟 (临时) */
    uint8_t data[] = { (uint8_t)(freq & 0xFF) };
    sendFrame(PB_CMD_LED_SET, data, 1);
}

void PetProtocol::sendMotor(uint8_t speed, int16_t steps) {
    /* paw_box 也没, 同样临时 */
    (void)speed; (void)steps;
}

void PetProtocol::sendSound(uint8_t sound_id) {
    /* paw_box 用 IR_NEC / IR_RAW, 临时占位 */
    uint8_t data[] = { sound_id };
    sendFrame(PB_CMD_LED_SET, data, 1);
}

void PetProtocol::sendAll(uint8_t expr, uint8_t led, uint8_t sound) {
    uint8_t data[] = { expr, led, sound };
    sendFrame(PB_CMD_SET_EXPR, data, 3);
}

void PetProtocol::sendQuerySensor() {
    sendFrame(PB_CMD_GET_TEMP, nullptr, 0);
}

void PetProtocol::sendQueryTime() {
    sendFrame(PB_CMD_GET_LIGHT, nullptr, 0);
}

void PetProtocol::sendPing() {
    sendFrame(PB_CMD_PING, nullptr, 0);
}

void PetProtocol::sendReset() {
    sendFrame(PB_CMD_PING, nullptr, 0);  /* 临时用 PING */
}

/* ==================== 回调注册 ==================== */

void PetProtocol::onSensorReport(SensorCallback cb) { _cb_sensor = cb; }
void PetProtocol::onEvent(EventCallback cb)         { _cb_event = cb; }
void PetProtocol::onAck(AckCallback cb)             { _cb_ack = cb; }
void PetProtocol::onPong(PongCallback cb)           { _cb_pong = cb; }
