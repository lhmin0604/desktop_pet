/************************************************************
 * 桌上宠物 - 屏幕表情显示 (PetDisplay.cpp)
 * 线稿+局部彩色版：黑/深灰细线 + 鼻/腮/内耳粉色填充
 * 特性: 8 心情 + 自动眨眼 + 心情装饰 + 顶部状态栏 HUD
 ************************************************************/

#include "PetDisplay.h"
#include "cat_bitmap.h"  /* 由 svg2catpath.py 从 image.svg 自动生成 (1-bit PROGMEM) */

/* ==================== 配色（线稿+局部彩色） ==================== */
static const uint32_t COL_BG       = 0xFAF7F2;   /* 米白背景（关键改动） */
static const uint32_t COL_LINE     = 0x333333;   /* 主描边（深灰，比纯黑柔和） */
static const uint32_t COL_NOSE     = 0xFF80AB;   /* 鼻头 + 内耳粉 */
static const uint32_t COL_CHEEK    = 0xFFB6C1;   /* 腮红淡粉 */
static const uint32_t COL_WHISKER  = 0x888888;   /* 胡须灰 */
static const uint32_t COL_TEXT     = 0x333333;   /* 文字 */
static const uint32_t COL_BAR_BG   = 0xE0E0E0;   /* 状态栏条底 */
static const uint32_t COL_BAR_FG   = 0x8BC34A;   /* 饱食度条绿 */
static const uint32_t COL_AFF      = 0xE53935;   /* 好感心形红 */
static const uint32_t COL_DECO_RED = 0xE53935;   /* ANGRY "!" */
static const uint32_t COL_DECO_PNK = 0xFF80AB;   /* LOVE 心 */
static const uint32_t COL_DECO_GRN = 0x66BB6A;   /* SICK 螺旋 */

/* ==================== 几何常量（圆胖方形脸，中心 160,130） ==================== */
static const int FACE_CX = 160;
static const int FACE_CY = 130;
static const int FACE_RX = 82;
static const int FACE_RY = 88;

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
    _lastHunger     = 255;
    _lastAffection  = 255;
    _lastLevel      = 255;
    _lastPhase      = 255;
    _phase          = 0;
    _nextBlink      = millis() + 3000;  /* 上电 3 秒后第一次眨眼 */
    _blinkStart     = 0;
    _angry          = false;

    Serial.printf("[屏幕] 已初始化，逻辑分辨率 %d x %d\n", _lcd.width(), _lcd.height());
}

/* ==================== 主渲染（带短路逻辑） ==================== */
void PetDisplay::render(PetMood mood, const PetStats& stats, unsigned long now) {
    /* 更新眨眼阶段（任何渲染前都更新） */
    uint8_t newPhase = updateBlinkPhase(now);

    /* 判断是否真的需要重画 */
    bool moodChanged   = (mood != _lastMood);
    bool statsChanged  = (stats.hunger != _lastHunger)
                      || (stats.affection != _lastAffection)
                      || (stats.level != _lastLevel);
    bool blinkChanged  = (newPhase != _lastPhase);

    if (!moodChanged && !statsChanged && !blinkChanged) {
        return;   /* 一切未变，省一次全屏重绘 */
    }

    /* 真重画：先更新缓存 */
    _lastMood      = mood;
    _lastHunger    = stats.hunger;
    _lastAffection = stats.affection;
    _lastLevel     = stats.level;
    _lastPhase     = newPhase;

    drawFace(mood, stats, now);
}

/* ==================== 眨眼状态机 ==================== */
uint8_t PetDisplay::updateBlinkPhase(unsigned long now) {
    /* 未到眨眼时刻 */
    if (now < _nextBlink) {
        _phase = 0;
        return 0;
    }
    /* 正在眨眼过程中 */
    if (_blinkStart == 0) {
        _blinkStart = _nextBlink;   /* 记录本次眨眼开始 */
    }
    unsigned long dt = now - _blinkStart;
    if (dt < 25)        _phase = 1;   /* 半闭 */
    else if (dt < 125)  _phase = 2;   /* 全闭 */
    else if (dt < 150)  _phase = 3;   /* 半开 */
    else {
        /* 一次眨眼结束，安排下一次 */
        _phase = 0;
        _blinkStart = 0;
        _nextBlink = now + 3000 + (random(0, 2000));
    }
    return _phase;
}

/* ==================== 整张脸（按心情分发） ==================== */
void PetDisplay::drawFace(PetMood mood, const PetStats& stats, unsigned long now) {
    _lcd.fillScreen(COL_BG);

    drawStatusBar(stats, now);

    if (mood == MOOD_SLEEPY) {
        /* Phase 1.7: SLEEPY 用完整身体矢量图(参考 image.svg) */
        drawSleepyCat();
        /* 猫本身已带 Zzz 文字,跳过 drawMoodLabel 避免重叠 */
    } else {
        /* 其他心情暂用旧版五官切换逻辑(待后续提供参考图后逐个替换为完整身体版) */
        drawHeadOutline();
        drawEars();
        drawEyes(mood, _phase);
        drawNose();
        drawMouth(mood);
        drawCheeks(mood);
        drawEyebrows();
        drawWhiskers();
        drawDecoration(mood);
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

/* ==================== 脸轮廓（单条椭圆描边替代四块填充） ==================== */
void PetDisplay::drawHeadOutline() {
    _lcd.drawEllipse(FACE_CX, FACE_CY, FACE_RX, FACE_RY, COL_LINE);
}

/* ==================== 耳朵（描边三角形 + 内部粉三角） ==================== */
void PetDisplay::drawEars() {
    /* 左耳：顶点 (90,52)，底边 (70,90)→(120,90) */
    _lcd.drawTriangle( 90, 52,  70, 90, 120, 90, COL_LINE);
    _lcd.fillTriangle( 95, 68,  80, 86, 115, 86, COL_NOSE);

    /* 右耳：顶点 (230,52)，底边 (200,90)→(250,90) */
    _lcd.drawTriangle(230, 52, 200, 90, 250, 90, COL_LINE);
    _lcd.fillTriangle(225, 68, 205, 86, 240, 86, COL_NOSE);
}

/* ==================== 眼睛（心情+眨眼阶段） ==================== */
void PetDisplay::drawEyes(PetMood mood, uint8_t phase) {
    const int ex1 = 130, ex2 = 190, ey = 115;

    /* 眨眼阶段：1/3 画半闭弧，2 画全闭横线 */
    if (phase == 2) {
        for (int x : {ex1, ex2}) {
            _lcd.drawLine(x - 14, ey, x + 14, ey, COL_LINE);
            _lcd.drawLine(x - 14, ey + 1, x + 14, ey + 1, COL_LINE);  /* 2px 粗 */
        }
        return;
    }
    if (phase == 1 || phase == 3) {
        for (int x : {ex1, ex2}) {
            /* 半闭：上弧向下塌 */
            _lcd.drawLine(x - 12, ey - 4, x + 12, ey + 4, COL_LINE);
        }
        return;
    }

    /* phase == 0：按心情画 */
    switch (mood) {
        case MOOD_SLEEPY: {                       /* 半闭眼：下弧 ∪ */
            for (int x : {ex1, ex2}) {
                _lcd.drawArc(x, ey, 12, 12, 200, 340, COL_LINE);
            }
            break;
        }
        case MOOD_SICK: {                         /* X 眼 */
            for (int x : {ex1, ex2}) {
                _lcd.drawLine(x - 11, ey - 11, x + 11, ey + 11, COL_LINE);
                _lcd.drawLine(x + 11, ey - 11, x - 11, ey + 11, COL_LINE);
            }
            break;
        }
        case MOOD_LOVE: {                         /* 爱心眼 */
            drawHeart(ex1, ey, 22, COL_DECO_PNK);
            drawHeart(ex2, ey, 22, COL_DECO_PNK);
            break;
        }
        case MOOD_EXCITED:                        /* 瞪圆 + 高光 */
        case MOOD_ANGRY:
        default: {                                /* 圆眼 + 瞳孔 + 高光 */
            for (int x : {ex1, ex2}) {
                _lcd.drawCircle(x, ey, 14, COL_LINE);
                _lcd.fillCircle(x, ey, 7, COL_LINE);
                _lcd.fillCircle(x + 3, ey - 3, 2, COL_BG);   /* 高光 */
            }
            break;
        }
    }
}

/* ==================== 鼻 ==================== */
void PetDisplay::drawNose() {
    _lcd.fillTriangle(152, 142, 168, 142, 160, 154, COL_NOSE);
}

/* ==================== 嘴 ==================== */
void PetDisplay::drawMouth(PetMood mood) {
    const int mx = 160, my = 170;
    switch (mood) {
        case MOOD_HAPPY:                        /* 弯弯笑：上弧 */
        case MOOD_EXCITED:
            _lcd.drawArc(mx, my, 22, 22, 20, 160, COL_LINE);
            break;
        case MOOD_NORMAL:                       /* 平线嘴 */
            _lcd.drawLine(mx - 16, my, mx + 16, my, COL_LINE);
            _lcd.drawLine(mx - 16, my + 1, mx + 16, my + 1, COL_LINE);
            break;
        case MOOD_HUNGRY:                       /* 小张开嘴 */
            _lcd.drawEllipse(mx, my, 9, 11, COL_LINE);
            break;
        case MOOD_SLEEPY:                       /* 小 O 嘴 */
            _lcd.drawCircle(mx, my + 2, 7, COL_LINE);
            break;
        case MOOD_ANGRY:                        /* 倒 V / 撇嘴 */
            _lcd.drawArc(mx, my + 12, 22, 22, 200, 340, COL_LINE);
            break;
        case MOOD_SICK:                         /* 耷拉撇嘴 */
            _lcd.drawArc(mx, my, 11, 11, 200, 340, COL_LINE);
            break;
        case MOOD_LOVE: {                       /* 心形嘴 */
            _lcd.fillCircle(mx - 5, my - 3, 5, COL_DECO_PNK);
            _lcd.fillCircle(mx + 5, my - 3, 5, COL_DECO_PNK);
            _lcd.fillTriangle(mx - 10, my, mx + 10, my, mx, my + 10, COL_DECO_PNK);
            break;
        }
        default:
            _lcd.drawArc(mx, my, 22, 22, 20, 160, COL_LINE);
            break;
    }
}

/* ==================== 腮红 ==================== */
void PetDisplay::drawCheeks(PetMood mood) {
    uint32_t c = 0;
    switch (mood) {
        case MOOD_EXCITED:
        case MOOD_LOVE:
        case MOOD_HAPPY:  c = COL_CHEEK; break;
        case MOOD_ANGRY:  c = COL_DECO_RED; break;
        default:          return;
    }
    _lcd.fillCircle(108, 150, 9, c);
    _lcd.fillCircle(212, 150, 9, c);
}

/* ==================== 倒眉（仅 ANGRY） ==================== */
void PetDisplay::drawEyebrows() {
    _angry = false;   /* 每次 drawFace 重新判断（在 drawEyes 里 mood==ANGRY 已处理） */
    /* 这里如果需要 ANGRY 画倒眉，逻辑可加： */
    /* 当前版本的 ANGRY 用圆眼+红腮已经表达，不再画额外倒眉保持线稿简洁 */
}

/* ==================== 胡须 ==================== */
void PetDisplay::drawWhiskers() {
    /* 左侧三根（错落） */
    _lcd.drawLine(96, 152,  56, 145, COL_WHISKER);
    _lcd.drawLine(96, 162,  60, 162, COL_WHISKER);
    _lcd.drawLine(96, 172,  58, 179, COL_WHISKER);
    /* 右侧三根（错落） */
    _lcd.drawLine(224, 152, 264, 145, COL_WHISKER);
    _lcd.drawLine(224, 162, 260, 162, COL_WHISKER);
    _lcd.drawLine(224, 172, 262, 179, COL_WHISKER);
}

/* ==================== 心情装饰（头顶浮动小图标） ==================== */
void PetDisplay::drawDecoration(PetMood mood) {
    _lcd.setFont(&fonts::Font2);
    _lcd.setTextDatum(textdatum_t::top_left);
    switch (mood) {
        case MOOD_SLEEPY:                       /* 头顶右上 Zzz */
            _lcd.setTextColor(COL_TEXT);
            _lcd.drawString("Zzz", 200, 36);
            break;
        case MOOD_LOVE: {                       /* 头顶左右各一颗小爱心 */
            drawSmallHeart(100, 50, 10, COL_DECO_PNK);
            drawSmallHeart(220, 50, 10, COL_DECO_PNK);
            break;
        }
        case MOOD_SICK:                         /* 头顶左上小螺旋 */
            drawSpiral(100, 52, 10, COL_DECO_GRN);
            break;
        case MOOD_ANGRY:                        /* 头顶左右"!" */
            _lcd.setFont(&fonts::Font4);
            _lcd.setTextColor(COL_DECO_RED);
            _lcd.drawString("!",  92, 32);
            _lcd.drawString("!", 218, 32);
            break;
        case MOOD_HUNGRY:                       /* 头顶"?" */
            _lcd.setFont(&fonts::Font4);
            _lcd.setTextColor(COL_TEXT);
            _lcd.setTextDatum(textdatum_t::top_center);
            _lcd.drawString("?", 160, 32);
            _lcd.setTextDatum(textdatum_t::top_left);
            break;
        default:
            break;
    }
}

/* ==================== 心情文字标签（底部居中） ==================== */
void PetDisplay::drawMoodLabel(PetMood mood) {
    _lcd.setTextDatum(textdatum_t::bottom_center);
    _lcd.setFont(&fonts::Font4);
    _lcd.setTextColor(COL_TEXT);
    _lcd.drawString(moodName(mood), 160, 230);
    _lcd.setTextDatum(textdatum_t::top_left);
}

/* ==================== 工具: 大爱心 ==================== */
void PetDisplay::drawHeart(int cx, int cy, int s, uint32_t c) {
    _lcd.fillCircle(cx - s / 2, cy - s / 3, s / 2, c);
    _lcd.fillCircle(cx + s / 2, cy - s / 3, s / 2, c);
    _lcd.fillTriangle(cx - s, cy, cx + s, cy, cx, cy + s, c);
}

/* ==================== 工具: 小爱心（描边版） ==================== */
void PetDisplay::drawSmallHeart(int cx, int cy, int s, uint32_t c) {
    int r = s / 2;
    _lcd.fillCircle(cx - r / 2, cy - r / 3, r, c);
    _lcd.fillCircle(cx + r / 2, cy - r / 3, r, c);
    _lcd.fillTriangle(cx - s, cy, cx + s, cy, cx, cy + s, c);
}

/* ==================== 工具: 螺旋（4 段半径递减的弧拼成蚊香形） ==================== */
void PetDisplay::drawSpiral(int cx, int cy, int r, uint32_t c) {
    _lcd.drawArc(cx, cy, r,         r,         0,   270, c);
    _lcd.drawArc(cx, cy, r * 3 / 4, r * 3 / 4, 90,  360, c);
    _lcd.drawArc(cx, cy, r / 2,     r / 2,     180, 450, c);
    _lcd.drawArc(cx, cy, r / 4,     r / 4,     270, 540, c);
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

/* ==================== SLEEPY 完整身体矢量图（参考 image.svg） ====================
 * 从 image.svg 用 cairosvg 渲染 + 二值化 → 1-bit 位图 (cat_bitmap)
 * 屏幕布局: 320×240,顶部 24px 状态栏已由 drawFace 画过
 *           猫区: x=0..320, y=24..240,左上角对齐
 *
 * 渲染方式: 扫每行字节,bit=0 (黑) 调 drawPixel
 *   8640 字节扫一遍,~1615 黑点 → 约 5ms/帧
 *   数据存 PROGMEM,占用 8.4KB flash,运行时 0 RAM
 *
 * 重新生成命令: python svg2catpath.py
 */
void PetDisplay::drawSleepyCat() {
    const uint32_t INK = 0x222222;   /* 墨黑,比 COL_LINE 略深 */
    const int y0 = 24;               /* 状态栏 24px 高度 */

    for (int y = 0; y < CAT_BMP_H; y++) {
        for (int xb = 0; xb < CAT_BMP_ROW_BYTES; xb++) {
            uint8_t byte = pgm_read_byte(&cat_bitmap[y * CAT_BMP_ROW_BYTES + xb]);
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
