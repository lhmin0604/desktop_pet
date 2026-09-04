/************************************************************
 * 桌上宠物 - 通信协议处理器实现 (PetProtocol.cpp)
 ************************************************************/

#include "PetProtocol.h"

PetProtocol::PetProtocol()
    : _serial(nullptr),
      _rx_state(RX_WAIT_HEAD),
      _cb_sensor(nullptr),
      _cb_event(nullptr),
      _cb_ack(nullptr),
      _cb_pong(nullptr) {
}

void PetProtocol::begin(HardwareSerial* serial) {
    _serial = serial;
    _rx_state = RX_WAIT_HEAD;
}

/* ==================== 接收处理 ==================== */

void PetProtocol::process() {
    while (_serial && _serial->available()) {
        uint8_t byte = _serial->read();

        switch (_rx_state) {
            case RX_WAIT_HEAD:
                if (byte == FRAME_HEAD) {
                    _rx_chk = FRAME_HEAD;
                    _rx_state = RX_WAIT_CMD;
                }
                break;

            case RX_WAIT_CMD:
                _rx_cmd = byte;
                _rx_chk ^= byte;
                _rx_state = RX_WAIT_LEN;
                break;

            case RX_WAIT_LEN:
                _rx_len = byte;
                _rx_chk ^= byte;
                if (_rx_len > FRAME_MAX_DATA) {
                    _rx_state = RX_WAIT_HEAD;  /* 长度超限，丢弃 */
                } else if (_rx_len == 0) {
                    _rx_state = RX_WAIT_CHK;
                } else {
                    _rx_data_idx = 0;
                    _rx_state = RX_WAIT_DATA;
                }
                break;

            case RX_WAIT_DATA:
                _rx_data[_rx_data_idx++] = byte;
                _rx_chk ^= byte;
                if (_rx_data_idx >= _rx_len) {
                    _rx_state = RX_WAIT_CHK;
                }
                break;

            case RX_WAIT_CHK:
                if (byte == _rx_chk) {
                    processFrame();
                }
                _rx_state = RX_WAIT_HEAD;
                break;
        }
    }
}

void PetProtocol::processFrame() {
    switch (_rx_cmd) {
        case CMD_SENSOR_REPORT:
            if (_rx_len >= 6 && _cb_sensor) {
                uint16_t temp = (_rx_data[0] << 8) | _rx_data[1];
                uint16_t light = (_rx_data[2] << 8) | _rx_data[3];
                _cb_sensor(temp, light, _rx_data[4], _rx_data[5]);
            }
            break;

        case CMD_EVENT_REPORT:
            if (_rx_len >= 2 && _cb_event) {
                _cb_event(_rx_data[0], _rx_data[1]);
            }
            break;

        case CMD_ACK_OK:
            if (_rx_len >= 1 && _cb_ack) {
                _cb_ack(_rx_data[0], true);
            }
            break;

        case CMD_ACK_FAIL:
            if (_rx_len >= 2 && _cb_ack) {
                _cb_ack(_rx_data[0], false);
            }
            break;

        case CMD_SYS_PONG:
            if (_cb_pong) {
                _cb_pong();
            }
            break;

        case CMD_TIME_REPORT:
            /* TODO: 处理时间上报 */
            break;

        default:
            Serial.printf("[协议] 未知上行命令: 0x%02X\n", _rx_cmd);
            break;
    }
}

/* ==================== 发送函数 ==================== */

uint8_t PetProtocol::calcChecksum(uint8_t cmd, uint8_t len,
                                    const uint8_t* data) {
    uint8_t chk = FRAME_HEAD ^ cmd ^ len;
    for (uint8_t i = 0; i < len; i++) {
        chk ^= data[i];
    }
    return chk;
}

void PetProtocol::sendFrame(uint8_t cmd, const uint8_t* data, uint8_t len) {
    if (!_serial) return;
    if (len > FRAME_MAX_DATA) len = FRAME_MAX_DATA;

    uint8_t frame[FRAME_MAX_DATA + FRAME_OVERHEAD];
    uint8_t idx = 0;

    frame[idx++] = FRAME_HEAD;
    frame[idx++] = cmd;
    frame[idx++] = len;

    for (uint8_t i = 0; i < len; i++) {
        frame[idx++] = data[i];
    }

    frame[idx] = calcChecksum(cmd, len, data);
    idx++;

    _serial->write(frame, idx);
}

void PetProtocol::sendExpression(uint8_t expr_id) {
    uint8_t data[] = { expr_id };
    sendFrame(CMD_SET_EXPRESSION, data, 1);
}

void PetProtocol::sendCustomFace(uint8_t seg[8]) {
    sendFrame(CMD_SET_CUSTOM_FACE, seg, 8);
}

void PetProtocol::sendLed(uint8_t led_val) {
    uint8_t data[] = { led_val };
    sendFrame(CMD_SET_LED, data, 1);
}

void PetProtocol::sendBuzzer(uint16_t freq, uint16_t time_10ms) {
    uint8_t data[4];
    data[0] = (freq >> 8) & 0xFF;
    data[1] = freq & 0xFF;
    data[2] = (time_10ms >> 8) & 0xFF;
    data[3] = time_10ms & 0xFF;
    sendFrame(CMD_SET_BUZZER, data, 4);
}

void PetProtocol::sendMotor(uint8_t speed, int16_t steps) {
    uint8_t data[3];
    data[0] = speed;
    data[1] = (steps >> 8) & 0xFF;
    data[2] = steps & 0xFF;
    sendFrame(CMD_SET_MOTOR, data, 3);
}

void PetProtocol::sendSound(uint8_t sound_id) {
    uint8_t data[] = { sound_id };
    sendFrame(CMD_PLAY_SOUND, data, 1);
}

void PetProtocol::sendAll(uint8_t expr, uint8_t led, uint8_t sound) {
    uint8_t data[] = { expr, led, sound };
    sendFrame(CMD_SET_ALL, data, 3);
}

void PetProtocol::sendQuerySensor() {
    sendFrame(CMD_QUERY_SENSOR, nullptr, 0);
}

void PetProtocol::sendQueryTime() {
    sendFrame(CMD_QUERY_TIME, nullptr, 0);
}

void PetProtocol::sendPing() {
    sendFrame(CMD_SYS_PING, nullptr, 0);
}

void PetProtocol::sendReset() {
    sendFrame(CMD_SYS_RESET, nullptr, 0);
}

/* ==================== 回调注册 ==================== */

void PetProtocol::onSensorReport(SensorCallback cb) { _cb_sensor = cb; }
void PetProtocol::onEvent(EventCallback cb)         { _cb_event = cb; }
void PetProtocol::onAck(AckCallback cb)             { _cb_ack = cb; }
void PetProtocol::onPong(PongCallback cb)           { _cb_pong = cb; }
