#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SVG → ESP32 1-bit 位图 C 数组 批量转换脚本
==========================================
读取 8 个心情的 SVG (cat_pic/*.svg + image.svg for SLEEPY),
用 cairosvg 渲染 (完美抗锯齿) + 二值化为 1-bit 位图,
生成统一的 cat_bitmaps.h 含 8 个 PROGMEM 数组 + 查表

数据量: 8 × (320×216/8) = 8 × 8640 bytes = 67.5 KB PROGMEM
渲染:   ~1500-3000 黑点 × drawPixel ≈ 5-8ms/帧 per mood

用法:
  python svg2catpath.py            # 默认读全部 8 个 → cat_bitmaps.h
  python svg2catpath.py one.svg out.h   # 单文件模式(兼容旧调用)

心情 → SVG 映射 (name 直译 + very_happy → EXCITED):
  HAPPY     <- cat_pic/happy.svg
  NORMAL    <- cat_pic/normal.svg
  HUNGRY    <- cat_pic/hungry.svg
  SLEEPY    <- image.svg
  ANGRY     <- cat_pic/angry.svg
  SICK      <- cat_pic/ill.svg
  EXCITED   <- cat_pic/very_happy.svg
  LOVE      <- cat_pic/love.svg
"""

import sys
import os

# 屏幕参数 (留 24px 顶部状态栏,猫区 320×216)
SCREEN_W     = 320
SCREEN_H     = 240
STATUSBAR_H  = 24
CAT_W        = SCREEN_W                # 320
CAT_H        = SCREEN_H - STATUSBAR_H  # 216

# 渲染分辨率(2x 超采样再下采样,保留更多抗锯齿细节)
RENDER_SCALE = 2

# 二值化阈值 (PIL 灰度 0-255,< threshold = 黑)
THRESHOLD    = 128

# 心情 → SVG 路径 映射 (按 PetState.h 的 MOOD_* 顺序)
MOOD_SVG = [
    ("happy",   "cat_pic/happy.svg"),
    ("normal",  "cat_pic/normal.svg"),
    ("hungry",  "cat_pic/hungry.svg"),
    ("sleepy",  "image.svg"),
    ("angry",   "cat_pic/angry.svg"),
    ("sick",    "cat_pic/ill.svg"),
    ("excited", "cat_pic/very_happy.svg"),
    ("love",    "cat_pic/love.svg"),
]
N_MOODS = len(MOOD_SVG)

DEFAULT_OUTPUT = "ESP32/src/desktop_pet/cat_bitmaps.h"

# ============================================================
# 主流程
# ============================================================

def ensure_deps():
    try:
        import cairosvg  # noqa
        from PIL import Image  # noqa
    except ImportError:
        import subprocess
        print("[依赖] 缺 cairosvg/Pillow,正在自动安装...")
        subprocess.run(
            [sys.executable, "-m", "pip", "install", "cairosvg", "Pillow", "--quiet"],
            check=True
        )

def render_svg_to_pil(svg_path, w, h):
    import cairosvg
    import io
    from PIL import Image
    raw = cairosvg.svg2png(url=svg_path,
                           output_width=w * RENDER_SCALE,
                           output_height=h * RENDER_SCALE,
                           background_color='white')
    img = Image.open(io.BytesIO(raw)).convert('L')
    img = img.resize((w, h), Image.LANCZOS)
    return img

def to_1bit_bytes(img, threshold=THRESHOLD):
    """PIL L → MSB-first 1-bit bytes (行对齐到字节)"""
    bw = img.point(lambda p: 0 if p < threshold else 1, mode='1')
    return bw.tobytes()  # '1' 模式自动 MSB-first, 行对齐

def generate_header(mood_bmps, w, h, output_path):
    """
    mood_bmps: list of (mood_name, bmp_bytes) tuples
    """
    row_bytes = (w + 7) // 8
    n_bytes_per = row_bytes * h
    total_bytes = n_bytes_per * len(mood_bmps)

    lines = []
    lines.append('/* ============================================================')
    lines.append(' * 自动生成 from 8 个心情 SVG,不要手改')
    lines.append(' * 生成命令: python svg2catpath.py')
    lines.append(' * 源文件:')
    for name, path in MOOD_SVG:
        lines.append(f' *   {name:8s} <- {path}')
    lines.append(f' * 尺寸: {w} x {h}  格式: 1-bit MSB-first')
    lines.append(f' * 渲染缩放: {RENDER_SCALE}x 超采样 + LANCZOS 下采样 + 阈值 {THRESHOLD}')
    lines.append(f' * 单图: {n_bytes_per} bytes ({n_bytes_per/1024:.1f} KB)')
    lines.append(f' * 8 图: {total_bytes} bytes ({total_bytes/1024:.1f} KB) PROGMEM')
    lines.append(' * 渲染: PetDisplay::drawMoodBitmap(mood) 查表扫字节,bit=0 调 drawPixel')
    lines.append(' * ============================================================ */')
    lines.append('')
    lines.append('#pragma once')
    lines.append('#include <Arduino.h>')
    lines.append('')
    lines.append('/* 心情枚举 (与 PetState.h 的 MOOD_* 一致) */')
    lines.append('enum {')
    lines.append('    CAT_BMP_MOOD_HAPPY   = 0,')
    lines.append('    CAT_BMP_MOOD_NORMAL  = 1,')
    lines.append('    CAT_BMP_MOOD_HUNGRY  = 2,')
    lines.append('    CAT_BMP_MOOD_SLEEPY  = 3,')
    lines.append('    CAT_BMP_MOOD_ANGRY   = 4,')
    lines.append('    CAT_BMP_MOOD_SICK    = 5,')
    lines.append('    CAT_BMP_MOOD_EXCITED = 6,')
    lines.append('    CAT_BMP_MOOD_LOVE    = 7,')
    lines.append('    CAT_BMP_MOOD_COUNT   = 8')
    lines.append('};')
    lines.append('')
    lines.append(f'#define CAT_BMP_W         {w}')
    lines.append(f'#define CAT_BMP_H         {h}')
    lines.append(f'#define CAT_BMP_ROW_BYTES {row_bytes}')
    lines.append(f'#define CAT_BMP_BYTES     {n_bytes_per}')
    lines.append('')
    lines.append('/* MSB-first 1-bit: 0=猫身黑  1=背景白 (透明,不画) */')

    bytes_per_line = 16
    for mood_name, bmp_bytes in mood_bmps:
        # 统计黑点数
        n_black = sum(8 - bin(b).count('1') for b in bmp_bytes)
        lines.append(f'/* --- {mood_name.upper():8s} ({n_black} 黑点, {100*n_black/(w*h):.1f}%) --- */')
        lines.append(f'static const uint8_t cat_bitmap_{mood_name}[CAT_BMP_BYTES] PROGMEM = {{')
        for row in range(h):
            row_start = row * row_bytes
            row_data = bmp_bytes[row_start:row_start + row_bytes]
            line_parts = []
            for i in range(0, len(row_data), bytes_per_line):
                chunk = row_data[i:i+bytes_per_line]
                hex_str = ', '.join(f'0x{b:02X}' for b in chunk)
                line_parts.append(hex_str)
            lines.append(f'  /* row {row:3d} */  ' + ', '.join(line_parts) + ',')
        lines.append('};')
        lines.append('')

    lines.append('/* 查表: mood → bitmap pointer */')
    lines.append('static const uint8_t* const cat_bitmaps[CAT_BMP_MOOD_COUNT] PROGMEM = {')
    for mood_name, _ in mood_bmps:
        lines.append(f'    cat_bitmap_{mood_name},')
    lines.append('};')
    lines.append('')

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))

    return total_bytes

def main():
    # 单文件兼容模式
    if len(sys.argv) >= 3:
        input_path = sys.argv[1]
        output_path = sys.argv[2]
        if not os.path.exists(input_path):
            print(f"[错误] 找不到 {input_path}")
            sys.exit(1)
        ensure_deps()
        print(f"[1/2] cairosvg 渲染 {input_path} ...")
        img = render_svg_to_pil(input_path, CAT_W, CAT_H)
        print(f"[2/2] 二值化 + 写 {output_path}")
        bmp = to_1bit_bytes(img, THRESHOLD)
        # 包装成单元素列表
        generate_header([("sleepy", bmp)], CAT_W, CAT_H, output_path)
        print(f"      OK {len(bmp)} 字节")
        return

    # 批量模式
    output_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_OUTPUT

    print(f"[1/3] 检查依赖 ...")
    ensure_deps()

    print(f"[2/3] 渲染 {N_MOODS} 个 SVG → 1-bit 位图 ...")
    mood_bmps = []
    for mood_name, svg_path in MOOD_SVG:
        if not os.path.exists(svg_path):
            print(f"  [错误] 找不到 {svg_path}")
            sys.exit(1)
        print(f"  - {mood_name:8s} <- {svg_path}")
        img = render_svg_to_pil(svg_path, CAT_W, CAT_H)
        bmp = to_1bit_bytes(img, THRESHOLD)
        mood_bmps.append((mood_name, bmp))

    print(f"[3/3] 生成 {output_path} ...")
    total = generate_header(mood_bmps, CAT_W, CAT_H, output_path)
    print(f"      OK 8 张共 {total} 字节 ({total/1024:.1f} KB) PROGMEM")

    print(f"\n[完成] PetDisplay::drawMoodBitmap(mood) 用 cat_bitmaps[mood] 查表,扫字节 bit=0 调 drawPixel")

if __name__ == "__main__":
    main()
