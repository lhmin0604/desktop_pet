/*
 * main_app.c - 桌上宠物 STC-B 主程序 (SDCC)
 *
 * 硬件: STC-B 学习板 (STC15F2K60S2 / IAP15F2K61S2, LQFP44)
 * 连接: UART2(P1.0/P1.1) ↔ ESP32-S3-BOX-3B EXT口
 * 波特率: 9600 bps, 8N1
 *
 * 引脚分配 (原理图 Ver3):
 *   K1=P3.2  K2=P3.3  K3=P1.7(GPIO/ADC7)
 *   五向导航=P1.2(ADC2, 电阻分压)
 *   UART2=P1.0(RXD2)/P1.1(TXD2)
 *   数码管段码=P0.0-P0.7  位选=P2.0-P2.2(74HC138)
 *   LED=P2.3(ULN2003)  振动电机=P2.4
 *   蜂鸣器=P3.4
 *   振动传感器=P3.5(需上板确认)
 *   霍尔传感器=P2.5(需上板确认)
 *   温度=P1.6(ADC6)  光照=P1.7(ADC7)
 */
#include "stc15_sdcc.h"
#include "sys.h"
#include "uart2_ext.h"
#include "protocol_esp32.h"
#include "display.h"
#include "beep.h"
#include "keys.h"
#include "adc_drv.h"
#include "vib.h"
#include "hall.h"
#include "expression.h"

/* comm_esp32.c 导出 */
extern void comm_init(void);
extern void comm_poll(void);
extern void comm_set_handler(void (*handler)(CommFrame *));
extern void comm_send_frame(unsigned char cmd, const unsigned char *data,
                             unsigned char len);
extern void comm_send_ack(unsigned char orig_cmd);
extern void comm_send_nack(unsigned char orig_cmd, unsigned char error);
extern void comm_send_sensor_report(unsigned int temp, unsigned int light,
                                     unsigned char buttons, unsigned char flags);
extern void comm_send_event(unsigned char event_type, unsigned char event_data);

/* ==================== 传感器数据 ==================== */
static unsigned int sensor_temp = 0;
static unsigned int sensor_light = 0;
static unsigned char sensor_buttons = 0;
static unsigned char sensor_flags = 0;

static unsigned int sensor_report_cnt = 0;
#define SENSOR_REPORT_INTERVAL  50   /* 50 * 10ms = 500ms */

/* 导航键状态 (用于边沿检测) */
static unsigned char nav_prev = NAV_NONE;

/* ==================== 命令处理 ==================== */
static void on_command(CommFrame *frame)
{
    switch (frame->cmd)
    {
    case CMD_SET_EXPRESSION:
        if (frame->len >= 1)
        {
            expr_set_face(frame->data[0]);
            comm_send_ack(frame->cmd);
        }
        else
            comm_send_nack(frame->cmd, 0x01);
        break;

    case CMD_SET_CUSTOM_FACE:
        if (frame->len >= 8)
        {
            expr_set_custom(frame->data);
            comm_send_ack(frame->cmd);
        }
        else
            comm_send_nack(frame->cmd, 0x01);
        break;

    case CMD_SET_LED:
        if (frame->len >= 1)
        {
            expr_set_led(frame->data[0]);
            comm_send_ack(frame->cmd);
        }
        else
            comm_send_nack(frame->cmd, 0x01);
        break;

    case CMD_SET_BUZZER:
        if (frame->len >= 4)
        {
            unsigned int freq, time_ms;
            freq = ((unsigned int)frame->data[0] << 8) | frame->data[1];
            time_ms = ((unsigned int)frame->data[2] << 8) | frame->data[3];
            if (beep_status() == BEEP_FREE)
            {
                beep_set(freq, time_ms / 10);
                comm_send_ack(frame->cmd);
            }
            else
                comm_send_nack(frame->cmd, 0x02);
        }
        else
            comm_send_nack(frame->cmd, 0x01);
        break;

    case CMD_SET_MOTOR:
        /* TODO: Phase 4 步进电机 */
        comm_send_ack(frame->cmd);
        break;

    case CMD_PLAY_SOUND:
        if (frame->len >= 1)
        {
            expr_play_sound(frame->data[0]);
            comm_send_ack(frame->cmd);
        }
        else
            comm_send_nack(frame->cmd, 0x01);
        break;

    case CMD_SET_ALL:
        if (frame->len >= 3)
        {
            expr_set_all(frame->data[0], frame->data[1], frame->data[2]);
            comm_send_ack(frame->cmd);
        }
        else
            comm_send_nack(frame->cmd, 0x01);
        break;

    case CMD_QUERY_SENSOR:
        comm_send_sensor_report(sensor_temp, sensor_light,
                                 sensor_buttons, sensor_flags);
        break;

    case CMD_QUERY_TIME:
        /* TODO: DS1302 RTC 读取 */
        comm_send_ack(frame->cmd);
        break;

    case CMD_SYS_PING:
        comm_send_frame(CMD_SYS_PONG, 0, 0);
        break;

    case CMD_SYS_RESET:
        expr_set_face(EXPR_SMILE);
        expr_set_led(0x00);
        comm_send_ack(frame->cmd);
        break;

    default:
        comm_send_nack(frame->cmd, 0xFF);
        break;
    }
}

/* ==================== 事件处理 ==================== */

static void handle_keys(void)
{
    unsigned char ev;
    unsigned char i;

    for (i = 0; i < 3; i++)
    {
        ev = keys_get_event(i);
        if (ev == KEY_PRESS)
        {
            sensor_buttons |= (1 << i);
            comm_send_event(EVENT_KEY, i + 1);
        }
        else if (ev == KEY_RELEASE)
        {
            sensor_buttons &= ~(1 << i);
        }
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
        /* 新按下 */
        switch (dir)
        {
            case NAV_UP:     nav_code = 0x05; break;
            case NAV_DOWN:   nav_code = 0x02; break;
            case NAV_LEFT:   nav_code = 0x04; break;
            case NAV_RIGHT:  nav_code = 0x01; break;
            case NAV_CENTER: nav_code = 0x03; break;
            default:         nav_code = 0x00; break;
        }
        if (nav_code)
            comm_send_event(EVENT_NAV, nav_code);
    }
    nav_prev = dir;
}

static void handle_vib(void)
{
    if (vib_sensor_read() == VIB_QUAKE)
    {
        sensor_flags |= 0x01;
        comm_send_event(EVENT_VIB, 0x01);
    }
}

static void handle_hall(void)
{
    unsigned char h = hall_read();
    if (h == HALL_CLOSE)
    {
        sensor_flags |= 0x02;
        comm_send_event(EVENT_HALL, 0x01);
    }
}

/* ==================== 主程序 ==================== */
int main(void)
{
    /* 1. 系统定时器 (1ms 时基) */
    sys_init();

    /* 2. 串口通信 */
    comm_init();
    comm_set_handler(on_command);

    /* 3. 外设初始化 */
    keys_init();
    adc_init();
    vib_init();
    hall_init();
    expr_init();

    /* 4. 开机表情 */
    expr_set_face(EXPR_SMILE);
    expr_set_led(0x01);
    beep_set(1000, 10);   /* 1000Hz, 100ms */

    /* 5. 主循环 */
    while (1)
    {
        /* 通信处理 (随时) */
        comm_poll();

        /* 数码管刷新 (每2ms) */
        display_refresh();

        /* 蜂鸣器计时 */
        beep_tick();

        /* 10ms 任务 */
        if (sys_flag_10ms)
        {
            sys_flag_10ms = 0;

            /* 按键扫描 */
            keys_scan();
            handle_keys();

            /* ADC 采样 */
            adc_poll();

            /* 更新传感器缓存 */
            sensor_temp  = adc_get_temp();
            sensor_light = adc_get_light();

            /* 振动/霍尔 */
            handle_vib();
            handle_hall();

            /* 传感器定时上报 */
            sensor_report_cnt++;
            if (sensor_report_cnt >= SENSOR_REPORT_INTERVAL)
            {
                sensor_report_cnt = 0;
                comm_send_sensor_report(sensor_temp, sensor_light,
                                         sensor_buttons, sensor_flags);
                sensor_flags = 0;
            }

            /* 导航键处理 */
            handle_nav();
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
