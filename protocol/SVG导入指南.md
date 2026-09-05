# SVG 导入指南 (ESP32 宠物形象 · 8 心情批量版)

> 本文档详细说明如何把设计好的 **8 个心情 SVG** 矢量图导入到 ESP32 桌宠项目中,变成可在 320×240 ILI9342C 屏幕上显示的位图。
>
> **适用版本**: Phase 1.8+ (所有 8 个心情各一张矢量图,查表渲染)
> **工具链**: Inkscape / Figma (画图) → cairosvg (渲染) → Pillow (二值化) → `svg2catpath.py` (打包成 C 数组)

---

## 〇、为什么需要这步?

ESP32 屏幕只有 320×240 分辨率,而且没有 GPU。如果用 LovyanGFX 在运行时画矢量路径(贝塞尔曲线),会:
- **慢**: 1069 段 `drawLine` ≈ 22ms/帧,80ms 节流下已经吃满
- **糙**: 1px 直线没有抗锯齿,边缘全是锯齿
- **占 RAM**: 路径数据如果存 RAM 会浪费宝贵的运行时内存
- **难维护**: 8 个心情 × 几十段路径 = 几百行 C++ 代码

最佳方案:**用 cairosvg 预先把 SVG 渲成抗锯齿的 1-bit 位图**,8 张烧到 PROGMEM 里,ESP32 运行时按心情查表、扫位图、用 `drawPixel` 涂黑点。这样:
- **快** (5-8ms/帧,8 个都一样)
- **清晰** (cairosvg 抗锯齿直接保留)
- **省** (8 张共 67.5KB flash, 0 RAM)
- **易维护** (改 SVG → 重跑脚本,不用改 C++)

---

## 一、整体流程

```
┌──────────────────────────┐
│  1. 画 8 个 SVG          │  Inkscape / Figma / Adobe Illustrator
│  (手绘或导出)            │  ⚠️ 必须是真矢量,不能是"假矢量"
└────────────┬─────────────┘
             │ 8 个 .svg (放 cat_pic/ 目录,image.svg 留根目录)
             ▼
┌──────────────────────────┐
│  2. 跑脚本               │  python svg2catpath.py
│  (渲染+二值化+打包)      │  依赖: cairosvg, Pillow
└────────────┬─────────────┘
             │ cat_bitmaps.h (67.5KB, 8 × 1-bit PROGMEM)
             ▼
┌──────────────────────────┐
│  3. (强烈建议)反渲验证   │  python verify_bitmaps.py
│  (检查 8 张图对不对)     │  生成 cat_pic_preview/*.png + 拼图
└────────────┬─────────────┘
             │ PNG 拼图肉眼检查
             ▼
┌──────────────────────────┐
│  4. C++ 渲染             │  PetDisplay::drawMoodBitmap(mood)
│  (查表→扫位图→涂黑)     │  cat_bitmaps[mood] 查指针,扫字节
└────────────┬─────────────┘
             │ SPI 写到 LCD
             ▼
┌──────────────────────────┐
│  5. 屏幕上看到           │  按当前 PetMood 渲染对应位图
└──────────────────────────┘
```

---

## 二、8 个心情的 SVG 路径与命名

`svg2catpath.py` 内部维护了一张**心情 → SVG 路径**的映射表。改这个表就能改映射。

| 心情 (PetMood) | 枚举值 | 默认 SVG 路径 | 黑点数参考 | 备注 |
|---|---|---|---|---|
| HAPPY | 0 | `cat_pic/happy.svg` | ~2400 | 开心 |
| NORMAL | 1 | `cat_pic/normal.svg` | ~1400 | 普通 |
| HUNGRY | 2 | `cat_pic/hungry.svg` | ~3300 | 饥饿 |
| SLEEPY | 3 | `image.svg` | ~1600 | **SLEEPY 单独用根目录的 `image.svg`**,不放在 cat_pic/ |
| ANGRY | 4 | `cat_pic/angry.svg` | ~2200 | 生气 |
| SICK | 5 | `cat_pic/ill.svg` | ~3600 | 生病 |
| EXCITED | 6 | `cat_pic/very_happy.svg` | ~1300 | 超开心 |
| LOVE | 7 | `cat_pic/love.svg` | ~2700 | 恋爱/被摸 |

> **当前未使用** (在 cat_pic/ 里但没接到心情): `eating.svg` / `play.svg` / `stand_up.svg` / `stroke.svg`
> 这些是预留的"场景动作"图(吃/玩/站立/抚摸),等以后有需要可以扩展成新心情或动画帧。

---

## 三、SVG 文件要求(最容易踩的坑!)

### ✅ 必须是真矢量

正确的 SVG 长这样:
```xml
<svg viewBox="0 0 1508 795" ...>
  <path d="M929.14 114.5 C933.04 114.57 937.2 111.94 940.73 110.27 C..."
        fill="#040503"/>
</svg>
```
关键特征:
- `<path>` 标签
- `d=` 属性包含 **M / C / L / Z** 等命令
- 直接用文本编辑器打开能看到大量数字坐标

### ❌ 不能是"假矢量"

某些工具(尤其是 PDF 编辑器、在线转换器)导出的 SVG 其实是位图,长这样:
```xml
<svg viewBox="0 0 1508 795" ...>
  <image xlink:href="data:image/png;base64,iVBORw0KGgo..."
         width="1508" height="775"/>
</svg>
```
**判断方法**: 用文本编辑器打开 SVG,搜索 `path`。如果只有 `<image>` 标签,没有 `<path>`,就是位图,不能直接用。

### ❌ 不能用 PDF

PDF 几乎都是位图(尤其是从 Word/Excel 导出的)。**先转真矢量 SVG 再用脚本**。

### ✅ viewBox 比例无所谓

脚本会按 `output_width=320, output_height=216` 强制缩放。viewBox 比例不需要和 320:216 一致,cairosvg 会按比例 fit。

---

## 四、工具安装

只需要 cairosvg + Pillow 两个 Python 库。脚本会**自动检查并安装**:
```bash
pip install cairosvg Pillow
```

如果自动安装失败(比如 Windows 上缺 cairo 系统库),用 conda:
```bash
conda install -c conda-forge cairosvg pillow
```

> **Windows 用户注意**: cairosvg 依赖 Cairo 原生库。如果 `pip install cairosvg` 报 "cannot find cairo",装 GTK+ runtime 或用 conda。

---

## 五、运行脚本

### 批量模式(推荐)
```bash
python svg2catpath.py
```
读所有 8 个 SVG → 生成 `ESP32/src/desktop_pet/cat_bitmaps.h`。

### 单文件模式(调试用)
```bash
python svg2catpath.py cat_pic/happy.svg /tmp/happy.h
```

### 输出文件结构
```c
// cat_bitmaps.h 头部
enum {
    CAT_BMP_MOOD_HAPPY   = 0,
    CAT_BMP_MOOD_NORMAL  = 1,
    CAT_BMP_MOOD_HUNGRY  = 2,
    CAT_BMP_MOOD_SLEEPY  = 3,
    CAT_BMP_MOOD_ANGRY   = 4,
    CAT_BMP_MOOD_SICK    = 5,
    CAT_BMP_MOOD_EXCITED = 6,
    CAT_BMP_MOOD_LOVE    = 7,
    CAT_BMP_MOOD_COUNT   = 8
};

#define CAT_BMP_W         320
#define CAT_BMP_H         216
#define CAT_BMP_ROW_BYTES 40
#define CAT_BMP_BYTES     8640      // 40 * 216

// 8 个独立 PROGMEM 数组
static const uint8_t cat_bitmap_happy[CAT_BMP_BYTES]   PROGMEM = { ... };
static const uint8_t cat_bitmap_normal[CAT_BMP_BYTES]  PROGMEM = { ... };
// ... 共 8 个

// 查表: mood → bitmap pointer
static const uint8_t* const cat_bitmaps[CAT_BMP_MOOD_COUNT] PROGMEM = {
    cat_bitmap_happy,    // MOOD_HAPPY
    cat_bitmap_normal,   // MOOD_NORMAL
    cat_bitmap_hungry,   // MOOD_HUNGRY
    cat_bitmap_sleepy,   // MOOD_SLEEPY
    cat_bitmap_angry,    // MOOD_ANGRY
    cat_bitmap_sick,     // MOOD_SICK
    cat_bitmap_excited,  // MOOD_EXCITED
    cat_bitmap_love,     // MOOD_LOVE
};
```

**总大小**: 8 × 8640 + 查表 16 字节 ≈ **67.5 KB PROGMEM**。

---

## 六、调整参数

打开 `svg2catpath.py`,顶部 4 个常量决定渲染质量:

```python
SCREEN_W     = 320         # 屏幕宽
SCREEN_H     = 240         # 屏幕高
STATUSBAR_H  = 24          # 顶部状态栏,猫区 = 240-24 = 216
CAT_W        = SCREEN_W    # 320
CAT_H        = SCREEN_H - STATUSBAR_H  # 216
RENDER_SCALE = 2           # 2x 超采样 (再下采样,保抗锯齿)
THRESHOLD    = 128         # 二值化阈值 (< 这个值 = 黑)
```

- **`RENDER_SCALE=2`**: 先渲 640×432,再 LANCZOS 下采到 320×216。比例越大越清晰,但越慢(脚本运行时)
- **`THRESHOLD=128`**: 灰度 < 128 算黑,>= 128 算白。SVG 线条细 → 调大(150),线条粗 → 调小(100)
- **改 `STATUSBAR_H`**: 状态栏高度变了(比如改成 32),猫区高度自动跟着变

---

## 七、C++ 集成

### 1. 包含头文件
```cpp
#include "PetDisplay.h"
#include "cat_bitmaps.h"  // 由 svg2catpath.py 自动生成
```

### 2. 渲染调用

`PetDisplay::drawMoodBitmap(mood)` 已经按 `cat_bitmaps[mood]` 查表,你**不需要手动写任何渲染代码**。

```cpp
// 在 PetDisplay.cpp 的 drawFace() 里:
void PetDisplay::drawFace(PetMood mood, const PetStats& stats, unsigned long now) {
    _lcd.fillScreen(COL_BG);
    drawStatusBar(stats, now);
    drawMoodBitmap(mood);   // 查表→画当前心情的位图
    drawMoodLabel(mood);    // 底部心情文字 (e.g. "HAPPY :)")
}
```

### 3. drawMoodBitmap 实现(参考)
```cpp
void PetDisplay::drawMoodBitmap(PetMood mood) {
    if ((uint8_t)mood >= CAT_BMP_MOOD_COUNT) return;
    const uint32_t INK = 0x222222;
    const int y0 = 24;  // 状态栏高度

    // 查表: cat_bitmaps[] 本身在 PROGMEM,要 pgm_read_ptr 读
    const uint8_t* bmp = (const uint8_t*)pgm_read_ptr(&cat_bitmaps[mood]);

    for (int y = 0; y < CAT_BMP_H; y++) {
        for (int xb = 0; xb < CAT_BMP_ROW_BYTES; xb++) {
            uint8_t byte = pgm_read_byte(&bmp[y * CAT_BMP_ROW_BYTES + xb]);
            if (byte == 0xFF) continue;
            for (int bit = 7; bit >= 0; bit--) {
                int x = xb * 8 + (7 - bit);
                if (x >= CAT_BMP_W) break;
                if (((byte >> bit) & 1) == 0) {  // 0 = 黑
                    _lcd.drawPixel(x, y0 + y, INK);
                }
            }
        }
    }
}
```

---

## 八、(必做!)反渲验证

**烧固件前必须验证位图反渲出来是啥样**。否则可能:
- SVG 写错导致某行/某列全白
- THRESHOLD 不对导致整张图全黑/全白
- 某个 path 漏渲染

`verify_bitmaps.py` 解析 `cat_bitmaps.h` → 反渲成 8 张 PNG + 一张拼图:

```bash
python verify_bitmaps.py
```

输出:
- `cat_pic_preview/happy.png` 等 8 张 320×216 单图
- `cat_pic_preview/_all_moods.png` 4×2 缩放拼图(关键看这个!)

**对比看拼图**:
- 每只猫应该清晰可辨
- 5 只差别大(开心/饿/病/怒/睡),3 只相似(普通/超开心/恋爱)
- 没有整张全黑/全白的(那是脚本坏了)
- 没有残影/锯齿严重(那是 SVG 不是真矢量)

如果某张图不对,改对应的 SVG,再跑 `svg2catpath.py` 和 `verify_bitmaps.py`。

---

## 九、踩过的坑(必读!)

| 坑 | 表现 | 排查 |
|---|---|---|
| **SVG 是位图** (`<image>` 标签) | cairosvg 抛 "no svg" 或输出全白 | 文本编辑器打开,搜 `<path>` |
| **viewBox 比例严重失调** | 猫挤到一角/被裁 | 改 SVG 的 viewBox 或调整 RENDER_SCALE |
| **THRESHOLD 不对** | 整张黑/白/花 | 改 `THRESHOLD` 调 100/150 试试 |
| **漏写闭合** (缺 `Z`) | 路径不闭合/不填色 | 检查 `<path d=` 最后有 `Z` |
| **忘记重跑脚本** | 改了 SVG,屏幕没变 | `python svg2catpath.py` + 重新编译 |
| **忘改 `#define` 常量** | 编译报"数组越界"或"未声明" | 同步改 STATUSBAR_H 等 |

---

## 十、换图流程(总结)

1. **准备 8 个 SVG** → 放 `cat_pic/` (SLEEPY 用 `image.svg` 在根目录)
2. **跑脚本** → `python svg2catpath.py` (生成 `cat_bitmaps.h`)
3. **反渲验证** → `python verify_bitmaps.py` → 看 `cat_pic_preview/_all_moods.png`
4. **不满意?改 SVG,回到 1**
5. **满意?编译烧固件** → 看 ESP32 屏幕

如果新加一个心情(比如 `MOOD_PLAY`):
1. 在 `PetState.h` 的 enum 加 `MOOD_PLAY`
2. 在 `svg2catpath.py` 的 `MOOD_SVG` 列表加 `("play", "cat_pic/play.svg")`
3. 在 `cat_bitmaps.h` 的 enum 块加 `CAT_BMP_MOOD_PLAY = 8, CAT_BMP_MOOD_COUNT = 9`
4. 跑脚本 + 验证
5. `PetDisplay::drawMoodBitmap(MOOD_PLAY)` 自动就支持了(因为 `cat_bitmaps[MOOD_PLAY]` 已存在)
