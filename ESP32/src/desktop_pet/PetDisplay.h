/************************************************************
 * 桌上宠物 - 屏幕表情显示 (PetDisplay.h)
 * 驱动 BOX-3B ILI9342C 彩屏，线稿+局部彩色风格绘制宠物小脸
 * 心情变 → 脸变；周期性眨眼；心情装饰；顶部状态栏 HUD
 *
 * 引脚 (BOX-3B 内部已接好，无需外接):
 *   SPI:  SCLK=7  MOSI=6  CS=5  DC=4  RST=悬空  BL=47
 *   (来源: LovyanGFX AutoDetect 中 ESP32_S3_BOX_V3 配置)
 ************************************************************/

#ifndef PET_DISPLAY_H
#define PET_DISPLAY_H

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "PetState.h"

/* BOX-3B 屏幕面板（显式配置，参照官方 AutoDetect） */
class LGFX : public lgfx::LGFX_Device {
public:
    lgfx::Bus_SPI       _bus;
    lgfx::Panel_ILI9342 _panel;
    LGFX(void);
};

class PetDisplay {
public:
    void begin();
    /* 渲染一帧；now=millis()，仅在 (心情变化 || 属性变化 || 眨眼阶段变化) 时真正重画 */
    void render(PetMood mood, const PetStats& stats, unsigned long now);

private:
    LGFX _lcd;

    /* 渲染缓存（用于检测变化决定是否重画） */
    PetMood _lastMood;
    uint8_t _lastHunger;
    uint8_t _lastAffection;
    uint8_t _lastLevel;
    uint8_t _lastPhase;       /* 上次重绘时的眨眼阶段 0..3 */

    /* 眨眼状态机 */
    unsigned long _nextBlink;  /* 下一次眨眼开始时刻 (millis) */
    unsigned long _blinkStart; /* 当前眨眼起始时刻 */
    uint8_t       _phase;      /* 0=睁 1=半闭 2=全闭 3=半开 */

    /* 当前心情是否带倒眉 */
    bool _angry;

    /* === 绘制子模块 === */
    void drawFace(PetMood mood, const PetStats& stats, unsigned long now);
    void drawStatusBar(const PetStats& stats, unsigned long now);
    void drawHeadOutline();
    void drawEars();
    void drawEyes(PetMood mood, uint8_t phase);
    void drawNose();
    void drawMouth(PetMood mood);
    void drawCheeks(PetMood mood);
    void drawEyebrows();
    void drawWhiskers();
    void drawDecoration(PetMood mood);
    void drawMoodLabel(PetMood mood);

    /* === 完整身体矢量图（每个心情一张，目前只实现 SLEEPY） === */
    void drawSleepyCat();   /* 完整身体版：圆胖方形 + 耳 + 眯眼 + ω嘴 + 卷尾 + Zzz */

    /* === 工具 === */
    void drawHeart(int cx, int cy, int s, uint32_t c);
    void drawSpiral(int cx, int cy, int r, uint32_t c);
    void drawSmallHeart(int cx, int cy, int s, uint32_t c);
    uint8_t updateBlinkPhase(unsigned long now);
    uint16_t cont(uint8_t r, uint8_t g, uint8_t b) { return _lcd.color888(r, g, b); }
    const char* moodName(PetMood mood);
};

#endif
