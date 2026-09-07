/*
 * main_app.c - 桌上宠物 STC-B 主程序 (SDCC + paw_box 485)
 *
 * 协议: paw_box [0xAA][0x55][ADDR][CMD][LEN][DATA][CRC8]
 * 物理: 板上 MAX485 (P3.6 RXD2 / P3.7 TXD2), RS485_DE = P3.7
 *       T2 定时器作波特率发生器, ISR 向量 8 / using(1)
 * 波特率: 9600 bps, 8N1
 *
 * 主循环结构 (避免 SDCC 跨 TU 函数指针 storage class 不匹配):
 *   1. comm_poll() 处理 485 接收, 收到完整主站命令置 comm_cmd_ready
 *   2. 检查 comm_cmd_ready, 调用 on_command() 处理, 清 flag
 *   3. 1ms 任务: 数码管扫描 + 蜂鸣器计时
 *   4. 10ms 任务: 按键 / ADC / 振动 / 霍尔 / 主动传感器上报 (500ms)
 *   5. 100ms 任务: 表情动画
 *
 * 下行命令 (ESP32 -> STC-B, CMD & 0x80 == 0):
 *   0x00 PING         -> PONG
 *   0x01 GET_TEMP     -> 上报 [temp, light_L, light_H]
 *   0x02 GET_LIGHT    -> 上报 [light_L, light_H]
 *   0x10 QUERY_SENSOR -> 主动上报 1 次
 *   0x20 LED_SET      -> [val] 设 LED
 *   0x21 SET_EXPR     -> [id]  切表情
 *   0x22 BUZZER       -> [freq_H, freq_L, time_H, time_L]
 *   0x23 PLAY_SOUND   -> [id]  播放音效
 *   0xF1 SYS_RESET    -> 重置
 *
 * 上行 (STC-B -> ESP32, CMD & 0x80 != 0):
 *   0x80 PONG         <- 心跳
 *   0xA0 EVENT        <- [type, data]
 *   0xE0 ACK_OK       <- [orig_cmd]
 *   0xE1 ACK_FAIL     <- [orig_cmd, err]
 */

#include "stc15_sdcc.h"
#include "sys.h"
#include "display.h"
#include "beep.h"
#include "keys.h"
#include "adc_drv.h"
#include "vib.h"
#include "hall.h"
#include "expression.h"
#include "bsp485_sdcc.h"
#include "protocol_sdcc.h"

/* comm_pawbox_sdcc.c 导出 */
extern void comm_init(void);
extern void comm_poll(void);
extern void comm_resp_send(unsigned char cmd,
                            const unsigned char *payload,
                            unsigned char len);
extern void comm_send_sensor(unsigned int temp, unsigned int light);
extern void comm_send_event(unsigned char event_type, unsigned char event_data);

extern volatile unsigned char comm_cmd_ready;
extern volatile unsigned char comm_last_cmd;
extern volatile unsigned char comm_last_len;
extern unsigned char comm_rx_payload[];

/* ============ 便捷应答 ============ */
static void send_ack(unsigned char orig_cmd)
{
    comm_resp_send(CMD_ACK_OK, &orig_cmd, 1);
}

static void send_nack(unsigned char orig_cmd, unsigned char err)
{
    unsigned char p[2];
    p[0] = orig_cmd;
    p[1] = err;
    comm_resp_send(CMD_ACK_FAIL, p, 2);
}

/* ============ 缓存的传感器值 ============ */
static unsigned int sensor_temp = 0;
static unsigned int sensor_light = 0;

/* 主动上报节流: 50 * 10ms = 500ms */
static unsigned int sensor_report_cnt = 0;
#define SENSOR_REPORT_INTERVAL  50

/* 导航/振动/霍尔状态: 用于边沿检测, 避免重复上报 */
static unsigned char nav_prev = NAV_NONE;
static unsigned char vib_prev = VIB_QUIET;
static unsigned char hall_prev = HALL_AWAY;

/* ============ 主站命令处理 ============ */
static void on_command(unsigned char cmd, unsigned char len)
{
    unsigned char p[3];
    unsigned int freq, time_10ms;

    switch (cmd)
    {
    case CMD_PING:
        comm_resp_send(CMD_PING, (const unsigned char *)0, 0);
        break;

    case CMD_GET_TEMP:
        p[0] = (unsigned char)(sensor_temp & 0xFF);
        p[1] = (unsigned char)(sensor_light & 0xFF);
        p[2] = (unsigned char)((sensor_light >> 8) & 0xFF);
        comm_resp_send(CMD_GET_TEMP, p, 3);
        break;

    case CMD_GET_LIGHT:
        p[0] = (unsigned char)(sensor_light & 0xFF);
        p[1] = (unsigned char)((sensor_light >> 8) & 0xFF);
        comm_resp_send(CMD_GET_LIGHT, p, 2);
        break;

    case CMD_QUERY_SENSOR:
        /* 主动触发 1 次上报 */
        comm_send_sensor(sensor_temp, sensor_light);
        break;

    case CMD_LED_SET:
        if (len >= 1) {
            expr_set_led(comm_rx_payload[0]);
            send_ack(cmd);
        } else {
            send_nack(cmd, 0x01);
        }
        break;

    case CMD_SET_EXPR:
        if (len >= 1) {
            expr_set_face(comm_rx_payload[0]);
            send_ack(cmd);
        } else {
            send_nack(cmd, 0x01);
        }
        break;

    case CMD_BUZZER:
        if (len >= 4) {
            freq = ((unsigned int)comm_rx_payload[0] << 8) | comm_rx_payload[1];
            time_10ms = ((unsigned int)comm_rx_payload[2] << 8) | comm_rx_payload[3];
            if (freq > 0 && time_10ms > 0) {
                beep_set(freq, time_10ms);
                send_ack(cmd);
            } else {
                send_nack(cmd, 0x02);
            }
        } else {
            send_nack(cmd, 0x01);
        }
        break;

    case CMD_PLAY_SOUND:
        if (len >= 1) {
            expr_play_sound(comm_rx_payload[0]);
            send_ack(cmd);
        } else {
            send_nack(cmd, 0x01);
        }
        break;

    case CMD_SYS_RESET:
        expr_set_face(EXPR_SMILE);
        expr_set_led(0x00);
        sensor_report_cnt = 0;
        nav_prev = NAV_NONE;
        vib_prev = VIB_QUIET;
        hall_prev = HALL_AWAY;
        send_ack(cmd);
        break;

    case CMD_QUERY_TIME:
        /* TODO: DS1302 RTC 暂未接, 只回 ACK */
        send_ack(cmd);
        break;

    default:
        send_nack(cmd, 0xFF);
        break;
    }
}

/* ============ 事件处理 (10ms 任务) ============ */
static void handle_keys(void)
{
    unsigned char ev;
    unsigned char i;

    for (i = 0; i < 3; i++)
    {
        ev = keys_get_event(i);
        if (ev == KEY_PRESS)
        {
            /* K1=1, K2=2, K3=3 (跟 ESP32 EVENT_KEY 期望一致) */
            comm_send_event(EVENT_KEY, (unsigned char)(i + 1));
        }
        /* KEY_RELEASE 不上报 */
    }
}

static void handle_nav(void)
{
    unsigned int val;
    unsigned char dir;
    unsigned char nav_code;

    val = adc_read(ADC_CH_NAV);
    dir = adc_get_nav_dir(val);

    if (dir != NAV_NONE && dir != nav_prev)
    {
        /* 0=UP, 1=DOWN, 2=LEFT, 3=RIGHT, 4=CENTER */
        switch (dir)
        {
            case NAV_UP:     nav_code = 0x00; break;
            case NAV_DOWN:   nav_code = 0x01; break;
            case NAV_LEFT:   nav_code = 0x02; break;
            case NAV_RIGHT:  nav_code = 0x03; break;
            case NAV_CENTER: nav_code = 0x04; break;
            default:         nav_code = 0xFF; break;
        }
        comm_send_event(EVENT_NAV, nav_code);
    }
    nav_prev = dir;
}

static void handle_vib(void)
{
    unsigned char cur = vib_sensor_read();
    if (cur == VIB_QUAKE && vib_prev == VIB_QUIET)
    {
        comm_send_event(EVENT_VIB, 0x01);
    }
    vib_prev = cur;
}

static void handle_hall(void)
{
    unsigned char cur = hall_read();
    if (cur != hall_prev)
    {
        comm_send_event(EVENT_HALL, cur);
    }
    hall_prev = cur;
}

/* ============ 1ms 任务 (主循环里调) ============ */
static void tick_1ms(void)
{
    display_refresh();
    beep_tick();
}

/* ============ 主程序 ============ */
int main(void)
{
    /* 1. 1ms 系统时基 (Timer0) */
    sys_init();

    /* 2. 通信 (UART2 + 板上 MAX485) */
    comm_init();

    /* 3. 外设 */
    keys_init();
    adc_init();
    vib_init();
    hall_init();
    expr_init();

    /* 4. 开机表情 */
    expr_set_face(EXPR_SMILE);
    expr_set_led(0x01);
    beep_set(1000, 10);   /* 1kHz, 100ms 启动提示音 */

    /* 5. 主循环 */
    while (1)
    {
        /* 通信 (最高优先级) */
        comm_poll();

        /* 主站命令处理 (轮询方式) */
        if (comm_cmd_ready)
        {
            unsigned char cmd = comm_last_cmd;
            unsigned char len = comm_last_len;
            comm_cmd_ready = 0;
            on_command(cmd, len);
        }

        /* 1ms 任务 */
        tick_1ms();

        /* 10ms 任务 */
        if (sys_flag_10ms)
        {
            sys_flag_10ms = 0;

            keys_scan();
            handle_keys();

            adc_poll();
            sensor_temp  = adc_get_temp();
            sensor_light = adc_get_light();

            handle_vib();
            handle_hall();
            handle_nav();

            /* 500ms 主动上报 */
            sensor_report_cnt++;
            if (sensor_report_cnt >= SENSOR_REPORT_INTERVAL)
            {
                sensor_report_cnt = 0;
                comm_send_sensor(sensor_temp, sensor_light);
            }
        }

        /* 100ms 任务 */
        if (sys_flag_100ms)
        {
            sys_flag_100ms = 0;
            expr_animate();
        }
    }

    return 0;
}
