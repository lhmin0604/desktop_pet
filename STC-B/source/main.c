/************************************************************
 * 桌上宠物 - STC-B 主控程序 (main.c)
 * Phase 1: 通信 + 表情系统
 *
 * 硬件: STC-B 学习板 (STC15F2K60S2)
 * 连接: UART2(EXT口) ↔ ESP32-S3-BOX-3B
 * 波特率: 9600 bps, 8N1
 ************************************************************/

#include "STC15F2K60S2.H"
#include "sys.H"
#include "displayer.H"
#include "Beep.h"
#include "Key.h"
#include "adc.h"
#include "DS1302.h"
#include "Vib.h"
#include "hall.H"
#include "comm.h"
#include "expression.h"

/* ==================== 系统声明 ==================== */
code unsigned long SysClock = 11059200;    /* 系统时钟 11.0592MHz */

/* ==================== decode_table ==================== */
/*
 * 索引 0-9:  数字 0-9
 * 索引 10:   全灭
 * 索引 11:   横杠 -
 * 索引 12:   T 形
 * 索引 13:   竖线 |
 * 索引 14:   < 形
 * 索引 15:   X 形
 * 索引 16:   ◠ (上弧/笑眼左)
 * 索引 17:   ‿ (下弧/微笑)
 * 索引 18:   ◡ (下弧/笑眼右)
 * 索引 19:   ‾ (上弧)
 * 索引 10-19 带小数点版本 = +16 (即 26-35)
 */
#ifdef _displayer_H_
code char decode_table[] = {
    /* 0-9 数字 */
    0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f,
    /* 10:全灭  11:横杠  12:T  13:竖线|  14:<  15:X */
    0x00, 0x40, 0x31, 0x30, 0x38, 0x76,
    /* 16:◠上弧  17:‿微笑  18:◡下弧  19:‾上横 */
    0x21, 0x08, 0x18, 0x01
};
#endif

/* ==================== 传感器数据缓存 ==================== */
static unsigned int sensor_temp = 0;      /* 温度 ADC 值 */
static unsigned int sensor_light = 0;     /* 光照 ADC 值 */
static unsigned char sensor_buttons = 0;  /* 按键状态 */
static unsigned char sensor_flags = 0;    /* 事件标志 */
static unsigned char sensor_dirty = 0;    /* 数据更新标志 */

/* 传感器定时上报计数器 */
static unsigned int sensor_timer = 0;
#define SENSOR_REPORT_INTERVAL  500   /* 每500ms上报一次 (10ms为单位) */

/* ==================== 命令处理 ==================== */
static void OnCommand(CommFrame *frame)
{
    switch(frame->cmd)
    {
        /* --- 设置表情 --- */
        case CMD_SET_EXPRESSION:
            if(frame->len >= 1)
            {
                ExprSetFace(frame->data[0]);
                CommSendAck(frame->cmd);
            }
            else
                CommSendNack(frame->cmd, 0x01);
            break;

        /* --- 自定义段码表情 --- */
        case CMD_SET_CUSTOM_FACE:
            if(frame->len >= 8)
            {
                ExprSetCustom(frame->data);
                CommSendAck(frame->cmd);
            }
            else
                CommSendNack(frame->cmd, 0x01);
            break;

        /* --- 设置LED --- */
        case CMD_SET_LED:
            if(frame->len >= 1)
            {
                ExprSetLed(frame->data[0]);
                CommSendAck(frame->cmd);
            }
            else
                CommSendNack(frame->cmd, 0x01);
            break;

        /* --- 设置蜂鸣器 --- */
        case CMD_SET_BUZZER:
            if(frame->len >= 4)
            {
                unsigned int freq, time;
                freq = ((unsigned int)frame->data[0] << 8) | frame->data[1];
                time = ((unsigned int)frame->data[2] << 8) | frame->data[3];
                if(GetBeepStatus() == enumBeepFree)
                {
                    SetBeep(freq, time);
                    CommSendAck(frame->cmd);
                }
                else
                    CommSendNack(frame->cmd, 0x02);  /* 蜂鸣器忙 */
            }
            else
                CommSendNack(frame->cmd, 0x01);
            break;

        /* --- 步进电机 --- */
        case CMD_SET_MOTOR:
            /* TODO: Phase 4 实现 */
            CommSendAck(frame->cmd);
            break;

        /* --- 播放音效 --- */
        case CMD_PLAY_SOUND:
            if(frame->len >= 1)
            {
                ExprPlaySound(frame->data[0]);
                CommSendAck(frame->cmd);
            }
            else
                CommSendNack(frame->cmd, 0x01);
            break;

        /* --- 一次设置全部 --- */
        case CMD_SET_ALL:
            if(frame->len >= 3)
            {
                ExprSetAll(frame->data[0], frame->data[1], frame->data[2]);
                CommSendAck(frame->cmd);
            }
            else
                CommSendNack(frame->cmd, 0x01);
            break;

        /* --- 查询传感器 --- */
        case CMD_QUERY_SENSOR:
            CommSendSensorReport(sensor_temp, sensor_light,
                                  sensor_buttons, sensor_flags);
            sensor_flags = 0;   /* 清除事件标志 */
            CommSendAck(frame->cmd);
            break;

        /* --- 查询时间 --- */
        case CMD_QUERY_TIME:
            {
                /* 读取 DS1302 时间并上报 */
                /* struct_RTC rtc = RTC_Read(); */
                /* CommSendTimeReport(rtc.year, rtc.month, ...); */
                /* TODO: 集成 DS1302 读取 */
                CommSendAck(frame->cmd);
            }
            break;

        /* --- 心跳 --- */
        case CMD_SYS_PING:
            {
                unsigned char pong = CMD_SYS_PONG;
                CommSendFrame(CMD_SYS_PONG, 0, 0);
            }
            break;

        /* --- 重置 --- */
        case CMD_SYS_RESET:
            ExprSetFace(EXPR_SMILE);
            ExprSetLed(0x00);
            CommSendAck(frame->cmd);
            break;

        default:
            CommSendNack(frame->cmd, 0xFF);   /* 未知命令 */
            break;
    }
}

/* ==================== 事件回调 ==================== */

/* 按键事件回调 */
static void OnKeyEvent(void)
{
    unsigned char key_act;

    /* K1 */
    key_act = GetKeyAct(enumKey1);
    if(key_act == enumKeyPress)
    {
        sensor_buttons |= 0x01;
        sensor_dirty = 1;
        CommSendEvent(EVENT_KEY, 0x01);
    }
    if(key_act == enumKeyRelease)
        sensor_buttons &= ~0x01;

    /* K2 */
    key_act = GetKeyAct(enumKey2);
    if(key_act == enumKeyPress)
    {
        sensor_buttons |= 0x02;
        sensor_dirty = 1;
        CommSendEvent(EVENT_KEY, 0x02);
    }
    if(key_act == enumKeyRelease)
        sensor_buttons &= ~0x02;

    /* K3 (注意: ADC启用后K3需通过 GetAdcNavAct 读取) */
    key_act = GetKeyAct(enumKey3);
    if(key_act == enumKeyPress)
    {
        sensor_buttons |= 0x04;
        sensor_dirty = 1;
        CommSendEvent(EVENT_KEY, 0x03);
    }
    if(key_act == enumKeyRelease)
        sensor_buttons &= ~0x04;
}

/* 导航按键事件回调 */
static void OnNavEvent(void)
{
    /* 导航按键方向上报 */
    unsigned char dir;

    if(GetAdcNavAct(enumAdcNavKeyUp) == enumKeyPress)
        CommSendEvent(EVENT_NAV, 0x05);
    if(GetAdcNavAct(enumAdcNavKeyDown) == enumKeyPress)
        CommSendEvent(EVENT_NAV, 0x02);
    if(GetAdcNavAct(enumAdcNavKeyLeft) == enumKeyPress)
        CommSendEvent(EVENT_NAV, 0x04);
    if(GetAdcNavAct(enumAdcNavKeyRight) == enumKeyPress)
        CommSendEvent(EVENT_NAV, 0x01);
    if(GetAdcNavAct(enumAdcNavKeyCenter) == enumKeyPress)
        CommSendEvent(EVENT_NAV, 0x03);
}

/* 振动传感器事件回调 */
static void OnVibEvent(void)
{
    if(GetVibAct() == enumVibQuake)
    {
        sensor_flags |= 0x01;   /* bit0 = 振动 */
        sensor_dirty = 1;
        CommSendEvent(EVENT_VIB, 0x01);
    }
}

/* 霍尔传感器事件回调 */
static void OnHallEvent(void)
{
    unsigned char hall = GetHallAct();
    if(hall == enumHallGetClose)
    {
        sensor_flags |= 0x02;   /* bit1 = 霍尔 */
        sensor_dirty = 1;
        CommSendEvent(EVENT_HALL, 0x01);
    }
    else if(hall == enumHallGetAway)
    {
        CommSendEvent(EVENT_HALL, 0x00);
    }
}

/* 10ms 定时事件回调（用于传感器采样和定时上报） */
static void On10msTick(void)
{
    struct_ADC adc_val;

    /* 每 10ms 更新一次传感器缓存 */
    adc_val = GetADC();
    sensor_temp = adc_val.Rt;
    sensor_light = adc_val.Rop;

    /* 定时上报计数器 */
    sensor_timer++;
    if(sensor_timer >= SENSOR_REPORT_INTERVAL / 10)
    {
        sensor_timer = 0;
        CommSendSensorReport(sensor_temp, sensor_light,
                              sensor_buttons, sensor_flags);
        sensor_flags = 0;   /* 上报后清除事件标志 */
    }
}

/* 100ms 定时事件回调（用于表情动画） */
static void On100msTick(void)
{
    ExprAnimate();
}

/* ==================== 主程序 ==================== */
void main()
{
    /* 1. 初始化表情引擎（数码管 + 蜂鸣器） */
    ExprInit();

    /* 2. 初始化按键 */
    KeyInit();

    /* 3. 初始化 ADC（含扩展口，启用温度/光照/导航按键） */
    AdcInit(ADCexpEXT);   /* 不使用EXT的ADC，保留EXT给UART2 */

    /* 4. 初始化 DS1302 实时时钟 */
    /* DS1302Init(default_time); */  /* TODO: 配置默认时间 */

    /* 5. 初始化串口通信 (UART2, 9600bps) */
    CommInit(9600);
    CommSetCmdHandler(OnCommand);

    /* 6. 注册事件回调 */
    SetEventCallBack(enumEventKey, OnKeyEvent);
    SetEventCallBack(enumEventNav, OnNavEvent);
    SetEventCallBack(enumEventVib, OnVibEvent);
    SetEventCallBack(enumEventHall, OnHallEvent);
    SetEventCallBack(enumEventSys10mS, On10msTick);
    SetEventCallBack(enumEventSys100mS, On100msTick);

    /* 7. 显示开机笑脸 */
    ExprSetFace(EXPR_SMILE);
    ExprSetLed(0x01);   /* L0亮，表示系统就绪 */

    /* 8. 播放开机音 */
    SetBeep(1000, 10);  /* 1000Hz, 100ms */

    /* 9. 系统初始化 */
    MySTC_Init();

    /* 10. 主循环 */
    while(1)
    {
        MySTC_OS();
    }
}
