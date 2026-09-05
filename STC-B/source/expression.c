/************************************************************
 * 桌上宠物 - 表情引擎实现 (expression.c)
 * 在8位数码管上显示宠物表情 + 蜂鸣器音效
 ************************************************************/

#include "STC15F2K60S2.H"
#include "sys.H"
#include "displayer.H"
#include "Beep.h"
#include "expression.h"

/* ==================== 表情段码表 ==================== */
/*
 * 8位数码管，每位8段（a-g + dp）
 * 用创意段码组合来表达表情
 *
 * 段码定义 (共阴极):
 *   bit0=a(上横) bit1=b(右上竖) bit2=c(右下竖) bit3=d(下横)
 *   bit4=e(左下竖) bit5=f(左上竖) bit6=g(中横) bit7=dp(小数点)
 *
 *   ──a──
 *  │     │
 *  f     b
 *  │     │
 *   ──g──
 *  │     │
 *  e     c
 *  │     │
 *   ──d── ·dp
 */

/* 自定义段码常量 */
#define SEG_NONE    0x00    /* 全灭 */
#define SEG_A       0x01    /* 上横 ─ */
#define SEG_B       0x02    /* 右上竖 │ */
#define SEG_C       0x04    /* 右下竖 │ */
#define SEG_D       0x08    /* 下横 ─ */
#define SEG_E       0x10    /* 左下竖 │ */
#define SEG_F       0x20    /* 左上竖 │ */
#define SEG_G       0x40    /* 中横 ─ */
#define SEG_DP      0x80    /* 小数点 · */

/* 组合段码 */
#define SEG_O       (SEG_A|SEG_B|SEG_C|SEG_D|SEG_E|SEG_F)       /* O形 */
#define SEG_U       (SEG_B|SEG_C|SEG_D|SEG_E)                    /* U形/笑嘴 */
#define SEG_N       (SEG_B|SEG_C|SEG_E|SEG_F)                    /* n形/眼睛 */
#define SEG_I       (SEG_E|SEG_F)                                 /* |竖线 */
#define SEG_T       (SEG_A|SEG_E|SEG_F)                           /* T形 */
#define SEG_L       (SEG_D|SEG_E|SEG_F)                           /* L形 */
#define SEG_DASH    (SEG_G)                                       /* 中横- */
#define SEG_DOT     (SEG_DP)                                      /* 点 */
#define SEG_LT      (SEG_D|SEG_E|SEG_G)                           /* < 形 */
#define SEG_GT      (SEG_B|SEG_C|SEG_G)                           /* > 形 */
#define SEG_X       (SEG_B|SEG_C|SEG_E|SEG_F|SEG_G)              /* X形 */
#define SEG_H       (SEG_B|SEG_C|SEG_E|SEG_F|SEG_G)             /* H形 */
#define SEG_Z       (SEG_A|SEG_B|SEG_D|SEG_E|SEG_G)             /* Z形 */
#define SEG_S       (SEG_A|SEG_C|SEG_D|SEG_F|SEG_G)             /* S形 */
#define SEG_3       (SEG_A|SEG_B|SEG_C|SEG_D|SEG_G)             /* 3形 */
#define SEG_V       (SEG_C|SEG_D|SEG_E)                           /* V/勾 */
#define SEG_WAVE    (SEG_C|SEG_D|SEG_E|SEG_G)                    /* ~波浪 */

/* 预设表情表: 每个表情 8 字节段码 */
code unsigned char face_table[EXPR_COUNT][8] = {
    /* 0x00 笑脸  ◠‿◠  */
    { SEG_A|SEG_F, SEG_A|SEG_B, SEG_NONE, SEG_NONE,
      SEG_NONE, SEG_NONE, SEG_C|SEG_D, SEG_D|SEG_E },

    /* 0x01 大笑  ^o^  */
    { SEG_A|SEG_F|SEG_B, SEG_O, SEG_NONE,
      SEG_A|SEG_B|SEG_F, SEG_NONE, SEG_NONE, SEG_NONE, SEG_NONE },

    /* 0x02 难过  T_T  */
    { SEG_T, SEG_NONE, SEG_NONE, SEG_NONE,
      SEG_T, SEG_NONE, SEG_NONE, SEG_NONE },

    /* 0x03 困倦  -.-  */
    { SEG_DASH, SEG_DOT, SEG_DASH, SEG_NONE,
      SEG_NONE, SEG_NONE, SEG_NONE, SEG_NONE },

    /* 0x04 生气  >_<  */
    { SEG_GT, SEG_NONE, SEG_A|SEG_D|SEG_G, SEG_NONE,
      SEG_LT, SEG_NONE, SEG_NONE, SEG_NONE },

    /* 0x05 惊讶  O_O  */
    { SEG_O, SEG_NONE, SEG_O, SEG_NONE,
      SEG_NONE, SEG_NONE, SEG_NONE, SEG_NONE },

    /* 0x06 爱心  <3<3  */
    { SEG_LT, SEG_A|SEG_B|SEG_G|SEG_C|SEG_D, SEG_LT,
      SEG_A|SEG_B|SEG_G|SEG_C|SEG_D, SEG_NONE, SEG_NONE, SEG_NONE, SEG_NONE },

    /* 0x07 吃饭  nom  */
    { SEG_E|SEG_F|SEG_G, SEG_O, SEG_E|SEG_F|SEG_G, SEG_NONE,
      SEG_NONE, SEG_NONE, SEG_NONE, SEG_NONE },

    /* 0x08 生病  x_x  */
    { SEG_X, SEG_NONE, SEG_X, SEG_NONE,
      SEG_NONE, SEG_NONE, SEG_NONE, SEG_NONE },

    /* 0x09 睡觉  zZz  */
    { SEG_Z, SEG_NONE, SEG_Z|SEG_A, SEG_NONE,
      SEG_Z, SEG_NONE, SEG_NONE, SEG_NONE },

    /* 0x0A 问好  Hi!  */
    { SEG_H, SEG_NONE, SEG_E|SEG_F, SEG_DOT,
      SEG_NONE, SEG_I, SEG_DOT, SEG_NONE },

    /* 0x0B 玩耍  ~_~  */
    { SEG_WAVE, SEG_NONE, SEG_WAVE, SEG_NONE,
      SEG_NONE, SEG_NONE, SEG_NONE, SEG_NONE }
};

/* ==================== 音效参数表 ==================== */
/* [频率Hz高字节, 频率Hz低字节, 时长高字节, 时长低字节] */
code unsigned int sound_table[][2] = {
    /* SOUND_SHORT  0x01 */ { 1000, 10 },   /* 1000Hz, 100ms */
    /* SOUND_HAPPY  0x02 */ { 1200, 20 },   /* 1200Hz, 200ms (后续会连续播) */
    /* SOUND_SAD    0x03 */ { 600,  30 },   /* 600Hz, 300ms */
    /* SOUND_ALARM  0x04 */ { 1500, 5  },   /* 1500Hz, 50ms */
    /* SOUND_EAT    0x05 */ { 800,  8  },   /* 800Hz, 80ms */
    /* SOUND_SLEEP  0x06 */ { 300,  40 },   /* 300Hz, 400ms */
    /* SOUND_WAKE   0x07 */ { 500,  15 },   /* 500Hz, 150ms */
    /* SOUND_LOVE   0x08 */ { 900,  25 },   /* 900Hz, 250ms */
};

/* ==================== 内部变量 ==================== */
static unsigned char current_expr = EXPR_SMILE;
static unsigned char anim_counter = 0;

/* ==================== 公共函数 ==================== */

void ExprInit(void)
{
    DisplayerInit();
    BeepInit();
    SetDisplayerArea(0, 7);     /* 使用全部8位数码管 */

    /* 显示默认笑脸 */
    ExprSetFace(EXPR_SMILE);
}

void ExprSetFace(unsigned char expr_id)
{
    unsigned char i;

    if(expr_id >= EXPR_COUNT)
        expr_id = EXPR_SMILE;   /* 越界保护 */

    current_expr = expr_id;

    /*
     * 注意: Seg7Print 使用的是 decode_table 索引
     * 这里我们使用自定义段码，不能直接传给 Seg7Print
     * 需要使用一种变通方式：
     * 方案A: 扩展 decode_table 加入表情段码
     * 方案B: 直接操作数码管扫描缓冲区（如果 BSP 暴露了接口）
     * 方案C: 利用 Seg7Print 传入 0-15 的索引，decode_table 前16项对应数字
     *
     * 这里采用方案A的思路：在 main.c 的 decode_table 中增加表情段码
     * 索引 16-31 用于表情段码。但 Seg7Print 只接受单字节参数。
     *
     * 实际实现时，需要根据 BSP 版本选择最佳方案。
     * 下面提供一个兼容的近似实现：用数字+符号近似表情。
     */

    /* 使用 Seg7Print 显示近似表情（数字索引到 decode_table） */
    switch(expr_id)
    {
        case EXPR_SMILE:    /* ◠‿◠ */
            Seg7Print(16, 17, 10, 10, 10, 10, 18, 19);
            break;
        case EXPR_LAUGH:    /* ^o^ */
            Seg7Print(16, 0, 16, 10, 10, 10, 10, 10);
            break;
        case EXPR_SAD:      /* T_T */
            Seg7Print(12, 10, 11, 12, 10, 10, 10, 10);
            break;
        case EXPR_SLEEPY:   /* -.- */
            Seg7Print(12, 13, 12, 10, 10, 10, 10, 10);
            break;
        case EXPR_ANGRY:    /* >_< */
            Seg7Print(14, 11, 15, 10, 10, 10, 10, 10);
            break;
        case EXPR_SURPRISE: /* O_O */
            Seg7Print(0, 10, 0, 10, 10, 10, 10, 10);
            break;
        case EXPR_LOVE:     /* <3<3 */
            Seg7Print(14, 3, 14, 3, 10, 10, 10, 10);
            break;
        case EXPR_EAT:      /* nom */
            Seg7Print(10, 10, 10, 10, 16, 0, 16, 10);
            break;
        case EXPR_SICK:     /* x_x */
            Seg7Print(15, 10, 15, 10, 10, 10, 10, 10);
            break;
        case EXPR_SLEEP:    /* zZz */
            Seg7Print(10, 10, 10, 10, 10, 16, 17, 16);
            break;
        case EXPR_HELLO:    /* Hi! */
            Seg7Print(10, 10, 10, 10, 10, 16, 1, 13);
            break;
        case EXPR_PLAY:     /* ~_~ */
            Seg7Print(17, 11, 17, 10, 10, 10, 10, 10);
            break;
        default:
            Seg7Print(16, 17, 10, 10, 10, 10, 18, 19);
            break;
    }
}

void ExprSetCustom(unsigned char *seg_data)
{
    /*
     * 自定义段码需要直接写入数码管扫描缓冲区
     * 具体实现取决于 BSP 版本是否暴露了底层接口
     * 这里留作扩展，目前使用 Seg7Print 近似
     */
    unsigned char i;
    /* TODO: 根据 BSP 版本实现直接段码写入 */
}

void ExprSetLed(unsigned char led_val)
{
    LedPrint(led_val);
}

void ExprPlaySound(unsigned char sound_id)
{
    unsigned int freq, time;

    if(sound_id < 1 || sound_id > 8)
        return;

    /* 特殊音效: 开心双音 */
    if(sound_id == SOUND_HAPPY)
    {
        if(GetBeepStatus() != enumBeepFree) return;
        /* 先播第一声，后续在定时回调中播第二声 */
        SetBeep(800, 10);   /* 800Hz, 100ms */
        /* TODO: 200ms 后再播 1200Hz */
        return;
    }

    /* 特殊音效: 低落下降音 */
    if(sound_id == SOUND_SAD)
    {
        if(GetBeepStatus() != enumBeepFree) return;
        SetBeep(800, 15);   /* 800Hz, 150ms */
        /* TODO: 300ms 后再播 400Hz */
        return;
    }

    /* 普通音效: 使用 sound_table */
    if(GetBeepStatus() != enumBeepFree) return;
    freq = sound_table[sound_id - 1][0];
    time = sound_table[sound_id - 1][1];
    SetBeep(freq, time);
}

void ExprSetAll(unsigned char expr_id, unsigned char led, unsigned char sound_id)
{
    ExprSetFace(expr_id);
    ExprSetLed(led);
    if(sound_id != 0)
        ExprPlaySound(sound_id);
}

void ExprAnimate(void)
{
    /* 表情动画帧更新（每 100ms 调用一次） */
    /* 目前预留，后续可加入眨眼、嘴巴动作等 */
    anim_counter++;

    /* 示例: 每5秒眨一次眼 */
    if(anim_counter >= 50)    /* 50 × 100ms = 5秒 */
    {
        anim_counter = 0;
        /* TODO: 短暂切换到闭眼表情再切回来 */
    }
}
