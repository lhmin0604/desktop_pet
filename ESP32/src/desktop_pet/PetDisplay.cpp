/************************************************************
 * 桌上宠物 - 屏幕表情显示 (PetDisplay.cpp)
 * 线稿+局部彩色版：黑/深灰细线 + 鼻/腮/内耳粉色填充
 * 特性: 8 心情 + 自动眨眼 + 心情装饰 + 顶部状态栏 HUD
 ************************************************************/

#include "PetDisplay.h"
#include "cat_bitmaps.h" /* 由 svg2catpath.py 从 8 个心情 SVG 自动生成 (1-bit PROGMEM) */

/* ==================== 配色 ==================== */
static const uint32_t COL_BG       = 0xFAF7F2;   /* 米白背景 */
static const uint32_t COL_LINE     = 0x333333;   /* 主描边（深灰） */
static const uint32_t COL_TEXT     = 0x333333;   /* 文字 */
static const uint32_t COL_BAR_BG   = 0xE0E0E0;   /* 状态栏条底 */
static const uint32_t COL_BAR_FG   = 0x8BC34A;   /* 饱食度条绿 */
static const uint32_t COL_AFF      = 0xE53935;   /* 好感心形红 */

/* ==================== LGFX 构造（BOX-3B 引脚，参照 AutoDetect） ==================== */
LGFX::LGFX(void) {
    {
        auto cfg = _bus.config();
        cfg.pin_mosi   = 6;
        cfg.pin_miso   = -1;
        cfg.pin_sclk   = 7;
        cfg.pin_dc     = 4;
        cfg.freq_write = 40000000;
        cfg.freq_read  = 16000000;
        cfg.spi_mode   = 0;
        cfg.spi_3wire  = true;      /* ILI9342C 用 3 线 SPI */
        _bus.config(cfg);
    }
    _panel.setBus(&_bus);

    {
        auto cfg = _panel.config();
        cfg.pin_cs           = 5;
        cfg.pin_rst          = -1;   /* RST 悬空（实测驱动 GPIO48 会整屏变白，暂不驱动） */
        cfg.offset_rotation  = 1;
        _panel.config(cfg);
        _panel.setRotation(1);
    }

    setPanel(&_panel);
}

/* ==================== 初始化 ==================== */
void PetDisplay::begin() {
    _lcd.init();
    /* BOX-3B 背光 GPIO47，高电平点亮 */
    pinMode(47, OUTPUT);
    digitalWrite(47, HIGH);
    _lcd.setSwapBytes(true);

    _lastMood       = (PetMood)255;
    _lastAction     = (PetAction)0xFF;
    _lastHunger     = 255;
    _lastAffection  = 255;
    _lastLevel      = 255;

    Serial.printf("[屏幕] 已初始化，逻辑分辨率 %d x %d\n", _lcd.width(), _lcd.height());
}

/* ==================== 主渲染（带短路逻辑） ==================== */
void PetDisplay::render(PetMood mood, PetAction action, const PetStats& stats, unsigned long now) {
    /* 判断是否真的需要重画 */
    bool moodChanged   = (mood != _lastMood);
    bool actionChanged = (action != _lastAction);
    bool statsChanged  = (stats.hunger != _lastHunger)
                      || (stats.affection != _lastAffection)
                      || (stats.level != _lastLevel);

    if (!moodChanged && !actionChanged && !statsChanged) {
        return;   /* 一切未变，省一次全屏重绘 */
    }

    /* 真重画：先更新缓存 */
    _lastMood      = mood;
    _lastAction    = action;
    _lastHunger    = stats.hunger;
    _lastAffection = stats.affection;
    _lastLevel     = stats.level;

    drawFace(mood, action, stats, now);
}

/* ==================== 整张脸（按动作/心情分发） ====================
 * 优先级: action ≠ ACT_NONE → 显示动作位图 + 动作文字
 *         action = ACT_NONE → 显示心情位图 + 心情文字
 */
void PetDisplay::drawFace(PetMood mood, PetAction action, const PetStats& stats, unsigned long now) {
    _lcd.fillScreen(COL_BG);

    drawStatusBar(stats, now);

    if (action != ACT_NONE) {
        drawActionBitmap(action);
        drawActionLabel(action);
    } else {
        drawMoodBitmap(mood);
        drawMoodLabel(mood);
    }
}

/* ==================== 顶部状态栏 (24px) ==================== */
void PetDisplay::drawStatusBar(const PetStats& stats, unsigned long now) {
    /* 底部分隔线 */
    _lcd.drawFastHLine(0, 24, _lcd.width(), COL_BAR_BG);

    /* 饱食度条：x=4..54, y=8..16 */
    _lcd.fillRect(4, 8, 50, 8, COL_BAR_BG);
    int barW = (stats.hunger * 50) / 100;
    if (barW > 0) _lcd.fillRect(4, 8, barW, 8, COL_BAR_FG);
    _lcd.drawRect(4, 8, 50, 8, COL_LINE);

    /* 饱食度百分比文字（Font2 小字） */
    _lcd.setFont(&fonts::Font2);
    _lcd.setTextColor(COL_TEXT);
    _lcd.setTextDatum(textdatum_t::top_left);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", stats.hunger);
    _lcd.drawString(buf, 60, 8);

    /* 好感心形（小心） */
    drawSmallHeart(120, 12, 7, COL_AFF);
    snprintf(buf, sizeof(buf), "%d", stats.affection);
    _lcd.drawString(buf, 132, 8);

    /* 等级 */
    snprintf(buf, sizeof(buf), "Lv.%d", stats.level);
    _lcd.drawString(buf, 180, 8);

    /* 时间占位：基于 millis() 显示已运行 mm:ss（后续接 DS1302） */
    unsigned long sec = (now / 1000) % 3600;
    unsigned long mm = sec / 60;
    unsigned long ss = sec % 60;
    snprintf(buf, sizeof(buf), "%02lu:%02lu", mm, ss);
    _lcd.setTextDatum(textdatum_t::top_right);
    _lcd.drawString(buf, _lcd.width() - 4, 8);
    _lcd.setTextDatum(textdatum_t::top_left);
}

/* ==================== 心情文字标签（底部居中） ==================== */
void PetDisplay::drawMoodLabel(PetMood mood) {
    _lcd.setTextDatum(textdatum_t::bottom_center);
    _lcd.setFont(&fonts::Font4);
    _lcd.setTextColor(COL_TEXT);
    _lcd.drawString(moodName(mood), 160, 230);
    _lcd.setTextDatum(textdatum_t::top_left);
}

/* ==================== 动作文字标签（底部居中） ==================== */
void PetDisplay::drawActionLabel(PetAction action) {
    _lcd.setTextDatum(textdatum_t::bottom_center);
    _lcd.setFont(&fonts::Font4);
    _lcd.setTextColor(COL_LINE);
    _lcd.drawString(actionName(action), 160, 230);
    _lcd.setTextDatum(textdatum_t::top_left);
}

/* ==================== 工具: 小爱心（用于状态栏） ==================== */
void PetDisplay::drawSmallHeart(int cx, int cy, int s, uint32_t c) {
    int r = s / 2;
    _lcd.fillCircle(cx - r / 2, cy - r / 3, r, c);
    _lcd.fillCircle(cx + r / 2, cy - r / 3, r, c);
    _lcd.fillTriangle(cx - s, cy, cx + s, cy, cx, cy + s, c);
}

/* ==================== 心情 => 底部文字标签 ==================== */
const char* PetDisplay::moodName(PetMood mood) {
    switch (mood) {
        case MOOD_HAPPY:   return "HAPPY :)";
        case MOOD_NORMAL:  return "NORMAL";
        case MOOD_HUNGRY:  return "HUNGRY ...";
        case MOOD_SLEEPY:  return "SLEEPY zZz";
        case MOOD_ANGRY:   return "ANGRY >:(";
        case MOOD_SICK:    return "SICK @.@";
        case MOOD_EXCITED: return "EXCITED !!";
        case MOOD_LOVE:    return "LOVE <3";
        default:           return "?";
    }
}

/* ==================== 动作 => 底部文字标签 ==================== */
const char* PetDisplay::actionName(PetAction act) {
    switch (act) {
        case ACT_EAT:       return "EATING...";
        case ACT_PLAY:      return "PLAYING!";
        case ACT_VIBRATION: return "POUND!!";
        case ACT_STROKE:    return "PURRING <3";
        default:            return "?";
    }
}

/* ==================== 8 心情 + 4 动作完整身体位图（统一从 cat_bitmaps.h 查表） ====================
 * 每个心情/动作的 SVG 由 svg2catpath.py 转成 1-bit PROGMEM 位图:
 *   8640 字节/张 × 12 (8 心情 + 4 动作) = 101.25 KB flash,运行时 0 RAM
 *   渲染: 查表 → 扫每行字节 → bit=0 (黑) 调 drawPixel
 *   速度: ~2000-3000 黑点 × drawPixel ≈ 5-8ms/帧
 *
 * 重新生成命令: python svg2catpath.py
 */
void PetDisplay::drawMoodBitmap(PetMood mood) {
    /* 越界保护(理论上不会到这里) */
    if ((uint8_t)mood >= CAT_BMP_MOOD_COUNT) return;

    const uint32_t INK = 0x222222;   /* 墨黑,比 COL_LINE 略深 */
    const int y0 = 24;               /* 状态栏 24px 高度 */

    /* 查表取出对应位图首地址(指针本身在 PROGMEM) */
    const uint8_t* bmp = (const uint8_t*)pgm_read_ptr(&cat_bitmaps_mood[mood]);

    for (int y = 0; y < CAT_BMP_H; y++) {
        for (int xb = 0; xb < CAT_BMP_ROW_BYTES; xb++) {
            uint8_t byte = pgm_read_byte(&bmp[y * CAT_BMP_ROW_BYTES + xb]);
            if (byte == 0xFF) continue;   /* 全白,跳过 */

            /* 0 = 黑,逐位检查 */
            for (int bit = 7; bit >= 0; bit--) {
                int x = xb * 8 + (7 - bit);
                if (x >= CAT_BMP_W) break;
                if (((byte >> bit) & 1) == 0) {   /* bit=0 = 猫身黑 */
                    _lcd.drawPixel(x, y0 + y, INK);
                }
            }
        }
    }
}

/* ==================== 动作位图（查 cat_bitmaps_act[act-1]） ==================== */
void PetDisplay::drawActionBitmap(PetAction act) {
    /* 越界保护 */
    if (act == ACT_NONE || (uint8_t)act >= ACT_COUNT) return;
    uint8_t idx = (uint8_t)act - 1;   /* ACT_EAT=1 → idx=0 */
    if (idx >= CAT_BMP_ACT_COUNT) return;

    const uint32_t INK = 0x222222;
    const int y0 = 24;

    /* 查表: cat_bitmaps_act[] 本身在 PROGMEM,需 pgm_read_ptr */
    const uint8_t* bmp = (const uint8_t*)pgm_read_ptr(&cat_bitmaps_act[idx]);

    for (int y = 0; y < CAT_BMP_H; y++) {
        for (int xb = 0; xb < CAT_BMP_ROW_BYTES; xb++) {
            uint8_t byte = pgm_read_byte(&bmp[y * CAT_BMP_ROW_BYTES + xb]);
            if (byte == 0xFF) continue;

            for (int bit = 7; bit >= 0; bit--) {
                int x = xb * 8 + (7 - bit);
                if (x >= CAT_BMP_W) break;
                if (((byte >> bit) & 1) == 0) {
                    _lcd.drawPixel(x, y0 + y, INK);
                }
            }
        }
    }
}
