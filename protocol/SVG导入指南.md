# SVG 导入指南 (ESP32 宠物形象)

> 本文档详细说明如何把设计好的 SVG 矢量图导入到 ESP32 桌宠项目中,变成可在 320×240 ILI9342C 屏幕上显示的位图。
>
> 适用版本: Phase 1.7+ (一个情绪一张矢量图)
> 工具链: Inkscape / Figma (画图) → cairosvg (渲染) → Pillow (二值化) → svg2catpath.py (打包成 C 数组)

---

## 〇、为什么需要这步?

ESP32 屏幕只有 320×240 分辨率,而且没有 GPU。如果用 LovyanGFX 在运行时画矢量路径(贝塞尔曲线),会:
- **慢**: 1069 段 drawLine ≈ 22ms/帧,80ms 节流下已经吃满
- **糙**: 1px 直线没有抗锯齿,边缘全是锯齿
- **占 RAM**: 路径数据如果存 RAM 会浪费宝贵的运行时内存

最佳方案:**用 cairosvg 预先把 SVG 渲成抗锯齿的 1-bit 位图**,烧到 PROGMEM 里,ESP32 运行时只扫描位图、用 `drawPixel` 涂黑点。这样:
- 快 (5ms/帧)
- 清晰 (cairosvg 抗锯齿直接保留)
- 省 (8.4KB flash, 0 RAM)

---

## 一、整体流程

```
┌────────────────────┐
│  1. 画 SVG         │  Inkscape / Figma / Adobe Illustrator
│  (手绘或导出)      │  ⚠️ 必须是真矢量,不能是"假矢量"
└─────────┬──────────┘
          │ image.svg (viewBox="0 0 W H")
          ▼
┌────────────────────┐
│  2. 跑脚本         │  python svg2catpath.py
│  (渲染+打包)       │  依赖: cairosvg, Pillow
└─────────┬──────────┘
          │ cat_bitmap.h (8.4KB, 1-bit PROGMEM)
          ▼
┌────────────────────┐
│  3. C++ 渲染       │  PetDisplay::drawSleepyCat()
│  (扫位图,涂黑)     │  扫字节,bit=0 调 drawPixel
└─────────┬──────────┘
          │ SPI 写到 LCD
          ▼
┌────────────────────┐
│  4. 屏幕上看到     │  ~5ms/帧, 1615 个黑点组成猫
└────────────────────┘
```

---

## 二、SVG 文件要求(最容易踩的坑!)

### ✅ 必须是真矢量

正确的 SVG 长这样:
```xml
<svg viewBox="0 0 1549 775" ...>
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
<svg viewBox="0 0 1549 775" ...>
  <image xlink:href="data:image/png;base64,iVBORw0KGgo..."
         width="1549" height="775"/>
</svg>
```
**判断方法**: 用文本编辑器打开 SVG,搜索 `path`。如果只有 `<image>` 标签,没有 `<path>`,就是位图,不能直接用。

### ❌ 不能用 PDF

`image.pdf` 看起来是矢量文件,但很多情况下 PDF 内嵌的是位图:

```bash
# 用 pdfimages 或文本编辑器查看 PDF 内容
pdftotext image.pdf - | head
# 或 strings image.pdf | grep ImageXObject
```

正确的 PDF 矢量内容: `<< /Type /Annot /Subtype /Line /... >>`
错误的位图内容: `<< /Type /XObject /Subtype /Image /Filter /FlateDecode >>`

**本项目踩过的坑**:
- `image.pdf` 1536×768 位图 (看起来像矢量但其实是 PNG 嵌进 PDF)
- `image.svg` 1549×775 真矢量 ✅

如果只有 PDF/位图,需要用 Inkscape 重新描一遍,或者用 potrace 之类的工具自动描。

### viewBox 大小不重要

- `viewBox="0 0 1549 775"` ✓ 任意大小,脚本会等比缩放到 320×216
- `viewBox="0 0 100 100"` ✓ 也能用
- 但**宽高比**最好接近 320:216 ≈ 1.48:1,否则会留很多白边

### 颜色

- 推荐:纯黑/深灰描边 (`#040503`, `#222222` 等)
- 避免:渐变、阴影、彩色填充(1-bit 位图只能二值化)
- 多色 SVG 会被脚本取灰度,只保留"够暗"的像素(灰度 < 128)

---

## 三、工具安装

### Python 依赖

```bash
pip install cairosvg Pillow
# 或一键装(脚本会自动检测并安装)
python svg2catpath.py
# 输出 [依赖] 缺 cairosvg/Pillow,正在自动安装... 时会自动跑 pip
```

### 系统依赖(cairosvg 需要 cairo 库)

- **Windows**: cairosvg 自带 wheel,无需额外装
- **macOS**: `brew install cairo pkg-config libffi`
- **Ubuntu/Debian**: `sudo apt install libcairo2-dev pkg-config python3-dev`
- **Fedora**: `sudo dnf install cairo-devel pkg-config python3-devel`

### 画图工具

| 工具 | 适用 | 备注 |
|------|------|------|
| **Inkscape** | 画矢量图 | 免费开源,导出 SVG 干净 |
| **Figma** | 设计/原型 | 免费版够用,导出 SVG 时**关闭** `Outline text` |
| **Adobe Illustrator** | 专业设计 | 导出 SVG 时选 "Save As" → SVG,不要 "Save for Web" |
| **纸笔 + 扫描** | 不会画图 | 扫描后用 potrace 矢量化 |

---

## 四、运行脚本

```bash
# 默认: 读 image.svg → cat_bitmap.h
python svg2catpath.py

# 指定输入输出
python svg2catpath.py my_cat.svg ESP32/src/desktop_pet/cat_bitmap_my.h
```

输出示例:
```
[1/4] 检查依赖 ...
[2/4] cairosvg 渲染 image.svg → 640x432 ...
[3/4] 二值化 (阈值 128) ...
      透明像素: 96.8%
[4/4] 生成 ESP32/src/desktop_pet/cat_bitmap.h ...
      OK 8640 字节 (8.4 KB) PROGMEM
```

### 调参(脚本顶部常量)

```python
SCREEN_W      = 320   # 屏幕宽
SCREEN_H      = 240   # 屏幕高
STATUSBAR_H   = 24    # 顶部状态栏高度(猫从 STATUSBAR_H 开始画)
CAT_W         = 320   # 猫图宽 (= 屏幕宽)
CAT_H         = 216   # 猫图高 (= 屏幕高 - 状态栏)
RENDER_SCALE  = 2     # 超采样倍数 (2 = 640×432 渲染再下采样)
THRESHOLD     = 128   # 二值化阈值 (灰度 < 阈值 = 黑)
```

**常见调参**:
- 猫太淡 → 调高 `THRESHOLD` (e.g. 100)
- 猫太粗/糊 → 调低 `THRESHOLD` (e.g. 160)
- 猫细节不够 → 调高 `RENDER_SCALE` (e.g. 3 → 4x 超采样)
- 状态栏高度变了 → 改 `STATUSBAR_H`

---

## 五、生成的文件结构

`cat_bitmap.h` 大致长这样:

```c
/* 自动生成 from image.svg, 不要手改 */
#pragma once
#include <Arduino.h>

#define CAT_BMP_W         320
#define CAT_BMP_H         216
#define CAT_BMP_ROW_BYTES 40          /* 320/8 = 40 字节/行 */
#define CAT_BMP_BYTES     8640        /* 40 * 216 = 8640 字节 */

static const uint8_t cat_bitmap[CAT_BMP_BYTES] PROGMEM = {
  /* row   0 */  0xFF, 0xFF, 0xFF, ..., 0xFF,
  /* row   1 */  0xFF, 0xFF, 0xFF, ..., 0xFF,
  ...
  /* row 215 */  0xFF, 0xFF, 0xFF, ..., 0xFF,
};
```

**位图格式**:
- 1-bit MSB-first
- **0 = 猫身黑** (画黑点)
- **1 = 背景白** (透明,不画,保留屏幕背景色)
- 行对齐到字节 (320 px / 8 = 40 bytes/row)

---

## 六、C++ 端集成

### 简单方案(推荐,稳如老狗)

`PetDisplay.cpp`:
```cpp
#include "cat_bitmap.h"

void PetDisplay::drawSleepyCat() {
    const uint32_t INK = 0x222222;  // 墨黑
    const int y0 = 24;              // 状态栏高度

    for (int y = 0; y < CAT_BMP_H; y++) {
        for (int xb = 0; xb < CAT_BMP_ROW_BYTES; xb++) {
            uint8_t byte = pgm_read_byte(&cat_bitmap[y * CAT_BMP_ROW_BYTES + xb]);
            if (byte == 0xFF) continue;  // 全白,跳过

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

**性能**: 1615 黑点 × ~3µs = ~5ms/帧
**优点**: LovyanGFX 兼容性最好,所有版本都支持
**缺点**: 像素调用次数多(虽然只画黑点)

### 进阶方案(run-length 优化)

如果发现上面太慢(理论上不会),可以用 `fillRect` 合并横向连续黑点:
```cpp
// 找连续 0 位,用 fillRect 一次画一条横线
_lcd.fillRect(x0 + run_start, y0 + y, x - run_start, 1, INK);
```
理论能快 20-30%,但代码复杂、边界 case 多,初版不推荐。

### ❌ 不要用 pushImage

虽然 LovyanGFX 有 `pushImage(x, y, w, h, data)` 一帧推完,但:
- 需要把 1-bit 位图转 RGB565,数据量从 8.4KB 暴涨到 135KB
- PROGMEM 占用变成 16x
- 透明色处理复杂(需要 0x0001 这种 magic value)
- 收益有限(5ms → 4ms)

**当前项目就是从 pushImage 方案退回 drawPixel 方案的**,见 §八 教训。

---

## 七、验证方法(必做!)

生成 `cat_bitmap.h` 后,在烧到 ESP32 前**一定要**先用 Python 验证位图数据正确:

```python
# 验证脚本 (保存为 verify_bmp.py)
import re
from PIL import Image

W, H, ROW_BYTES = 320, 216, 40

with open('ESP32/src/desktop_pet/cat_bitmap.h', 'r', encoding='utf-8') as f:
    content = f.read()

bytes_list = re.findall(r'0x([0-9A-Fa-f]{2})', content)
print(f'提取到 {len(bytes_list)} 字节 (期望 {ROW_BYTES * H})')
assert len(bytes_list) == ROW_BYTES * H, "字节数不匹配!"

img = Image.new('L', (W, H), 255)
pixels = img.load()
data = bytes(int(b, 16) for b in bytes_list)

for y in range(H):
    for xb in range(ROW_BYTES):
        byte = data[y * ROW_BYTES + xb]
        for bit in range(7, -1, -1):
            x = xb * 8 + (7 - bit)
            if x >= W: break
            if not ((byte >> bit) & 1):
                pixels[x, y] = 0

img.save('_verify_bmp.png')
print('OK -> _verify_bmp.png (用图片查看器打开看效果)')
```

**应该看到**: 一只清晰的小猫,黑线轮廓分明,跟 `image.svg` 在浏览器/Inkscape 里看到的一致。

**如果看到**:
- **全白** → 脚本出 bug 了,所有像素都被二值化成 1,调高 `THRESHOLD` 或检查 SVG 是否非空
- **全黑** → 阈值过低,所有像素都被当成猫身,调低 `THRESHOLD`
- **猫形状不对** → 比例不对,检查 SVG viewBox 是否设置正确
- **马赛克/锯齿** → `RENDER_SCALE` 太小,调到 3 或 4

---

## 八、踩过的坑(必读!)

### 坑 1: 用 PDF 当矢量源

**症状**: SVG 看起来"对"但渲出来是乱线/全白/全黑
**原因**: PDF 实际是位图 (`/Subtype /Image`),不是真矢量
**解决**: 用 Inkscape 重画或 `potrace` 矢量化,导出真 SVG

### 坑 2: Figma 导出 SVG 时把文字转曲线了

**症状**: 文字部分(REST 字符)在屏幕上显示不正常
**原因**: Figma 默认 "Outline text" 把文字转成 path,但路径数据可能有偏差
**解决**: Figma 导出 SVG 时**取消勾选** "Outline text",或者用系统字体重新嵌入

### 坑 3: 字节序错位,屏幕显示镜像

**症状**: 猫左右颠倒,或者像素错位
**原因**: MSB/LSB 弄反,或 LovyanGFX `setSwapBytes()` 跟数据格式不匹配
**解决**:
- 1-bit 位图无字节序问题(只有 RGB565 pushImage 才有)
- 如果用 RGB565,确认 `setSwapBytes(true)` + 数据大端

### 坑 4: 编译报 `'XXX' was not declared in this scope`

**症状**: 编译失败,找不到某个常量(如 `STATUSBAR_H`)
**原因**: 脚本(Python)里的常量没同步到 C++ 端
**解决**: 在 C++ 里用字面量,或者在 `.h` 文件里定义共享常量

### 坑 5: 状态栏被猫覆盖 / 猫画到状态栏里

**症状**: 猫显示在屏幕最顶端,或者状态栏看不见
**原因**: 忘了 `STATUSBAR_H` 偏移,猫从 y=0 开始画
**解决**: `y0 = STATUSBAR_H` (本项目是 24px)

### 坑 6: 屏白屏,完全没显示

**症状**: 上传后屏幕一片白/全黑/全蓝
**排查顺序**:
1. 编译有没有 0 error? (有 error 的话烧的还是旧固件)
2. 串口有没有 `[屏幕] OK`? (没有 = `display.begin()` 没跑)
3. `display.begin()` 之后有没有 `fillScreen(COL_BG)`? (没有 = 上次内容残留)
4. 是不是 `pgm_read_byte` 读到错数据? (检查 `cat_bitmap` 数组大小, 是否被编译器优化掉)
5. 用 `drawPixel` 替代 `writeFillRect`/`pushImage` 看能不能显示

### 坑 7: 屏闪/撕裂/残影

**症状**: 屏幕上看到上一帧的残留
**原因**: `fillScreen` 没调,或者 `drawFace` 被中断
**解决**: 每次 `render()` 都先 `fillScreen(COL_BG)`

---

## 九、其他 7 个心情怎么加?

照同样套路:
1. 画 `image_happy.svg`、`image_hungry.svg` ... 7 个文件
2. 跑脚本各生成一个 `.h`:
   ```bash
   python svg2catpath.py image_happy.svg cat_bitmap_happy.h
   python svg2catpath.py image_hungry.svg cat_bitmap_hungry.h
   ...
   ```
3. `PetDisplay.h` 加 7 个 `drawXxxCat()` 声明
4. `PetDisplay.cpp` 加 7 个 `drawXxxCat()` 实现(各 include 对应 `.h`)
5. `drawFace()` 里按 mood 分发:
   ```cpp
   if (mood == MOOD_SLEEPY) drawSleepyCat();
   else if (mood == MOOD_HAPPY) drawHappyCat();
   ...
   ```
6. **可优化**: 7 张图全用 1-bit,共 7×8.4 = 59KB PROGMEM,仍能接受

**如果想动态切换** (用户上传自己的宠物图):
- 把 1-bit 位图存到 SD 卡,运行时 `pgm_read_byte` → `drawPixel`
- 详见 Phase 3 规划(SD 卡 + 触摸)

---

## 十、参考链接

- [cairosvg 文档](https://cairosvg.org/documentation/)
- [Pillow 图像处理](https://pillow.readthedocs.io/)
- [Inkscape 导出 SVG 教程](https://inkscape.org/doc/tutorials/)
- [LovyanGFX drawPixel 文档](https://github.com/lovyan03/LovyanGFX)
- [本项目架构文档 §1.7](./屏幕显示架构.md#一七sleepy-完整身体矢量图phase-17)
- [svg2catpath.py](../svg2catpath.py) 源码
