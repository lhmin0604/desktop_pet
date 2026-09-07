/*
 * expression.c - 表情引擎 (SDCC)
 *
 * 8位共阴数码管显示宠物表情 + 蜂鸣器音效
 * 段码定义: a=bit0, b=bit1, c=bit2, d=bit3, e=bit4, f=bit5, g=bit6, dp=bit7
 */
#include "stc15_sdcc.h"
#include "display.h"
#include "beep.h"
#include "expression.h"

/* 段码常量 */
#define SA  0x01
#define SB  0x02
#define SC  0x04
#define SD  0x08
#define SE  0x10
#define SF  0x20
#define SG  0x40
#define SDP 0x80

/* 组合段码 */
#define SO  (SA|SB|SC|SD|SE|SF)
#define SU  (SB|SC|SD|SE)
#define SN  (SB|SC|SE|SF)
#define SI  (SE|SF)
#define ST  (SA|SE|SF)
#define SL  (SD|SE|SF)
#define SDASH (SG)
#define SDOT  (SDP)
#define SLT  (SD|SE|SG)
#define SGT  (SB|SC|SG)
#define SX  (SB|SC|SE|SF|SG)
#define SH  (SB|SC|SE|SF|SG)
#define SZ  (SA|SB|SD|SE|SG)
#define SS  (SA|SC|SD|SF|SG)
#define S3  (SA|SB|SC|SD|SG)
#define SV  (SC|SD|SE)
#define SWAVE (SC|SD|SE|SG)
#define SNONE 0x00

/* 预设表情表: 12个表情 x 8字节 = 96字节, 扁平化1D数组
 *
 * ★ 关键: 用 static const 强制放 CODE 区 (--model-large 下默认会进
 *   XDATA + XINIT 复制, 但 SDCC 链接器的 IHX 输出在某些情况下
 *   XINIT 偏移错位, 导致 face_table 读到的是 XDATA 随机值, 7-seg
 *   显示乱码. 改 const 后直接 MOVC 读, 绕过 XINIT, 100% 可靠). */
static const unsigned char face_table[96] = {
    /* 0x00 笑脸 */
    SA|SF, SA|SB, SNONE, SNONE, SNONE, SNONE, SC|SD, SD|SE,
    /* 0x01 大笑 */
    SA|SF|SB, SO, SNONE, SA|SB|SF, SNONE, SNONE, SNONE, SNONE,
    /* 0x02 难过 */
    ST, SNONE, SNONE, SNONE, ST, SNONE, SNONE, SNONE,
    /* 0x03 困倦 */
    SDASH, SDOT, SDASH, SNONE, SNONE, SNONE, SNONE, SNONE,
    /* 0x04 生气 */
    SGT, SNONE, SA|SD|SG, SNONE, SLT, SNONE, SNONE, SNONE,
    /* 0x05 惊讶 */
    SO, SNONE, SO, SNONE, SNONE, SNONE, SNONE, SNONE,
    /* 0x06 爱心 */
    SLT, SA|SB|SG|SC|SD, SLT, SA|SB|SG|SC|SD, SNONE, SNONE, SNONE, SNONE,
    /* 0x07 吃饭 */
    SE|SF|SG, SO, SE|SF|SG, SNONE, SNONE, SNONE, SNONE, SNONE,
    /* 0x08 生病 */
    SX, SNONE, SX, SNONE, SNONE, SNONE, SNONE, SNONE,
    /* 0x09 睡觉 */
    SZ, SNONE, SZ|SA, SNONE, SZ, SNONE, SNONE, SNONE,
    /* 0x0A 问好 */
    SH, SNONE, SE|SF, SDOT, SNONE, SI, SDOT, SNONE,
    /* 0x0B 玩耍 */
    SWAVE, SNONE, SWAVE, SNONE, SNONE, SNONE, SNONE, SNONE
};

/* 音效参数表 [freq, time_10ms]
 * 同样改 const 放 CODE 区, 避免 XINIT 复制问题 */
static const unsigned int sound_freq[8] = {
    1000, 1200, 600, 1500, 800, 300, 500, 900
};
static const unsigned int sound_time[8] = {
    1, 2, 3, 1, 1, 4, 2, 3
};

static unsigned char anim_counter = 0;

void expr_init(void)
{
    display_init();
    beep_init();
}

void expr_set_face(unsigned char expr_id)
{
    unsigned char i;
    unsigned char base;
    if (expr_id >= 12) expr_id = 0;
    base = expr_id * 8;
    for (i = 0; i < 8; i++)
        disp_segments[i] = face_table[base + i];
}

void expr_set_custom(unsigned char *seg_data)
{
    unsigned char i;
    for (i = 0; i < 8; i++)
        disp_segments[i] = seg_data[i];
}

void expr_set_led(unsigned char led_val)
{
    display_set_led(led_val);
}

void expr_play_sound(unsigned char sound_id)
{
    unsigned int freq, time_ms;
    if (sound_id < 1 || sound_id > 8) return;
    if (beep_status() != BEEP_FREE) return;

    freq = sound_freq[sound_id - 1];
    time_ms = sound_time[sound_id - 1];
    beep_set(freq, time_ms);
}

void expr_set_all(unsigned char expr_id, unsigned char led, unsigned char sound_id)
{
    expr_set_face(expr_id);
    expr_set_led(led);
    if (sound_id != 0)
        expr_play_sound(sound_id);
}

void expr_animate(void)
{
    anim_counter++;
    if (anim_counter >= 50)
        anim_counter = 0;
}
