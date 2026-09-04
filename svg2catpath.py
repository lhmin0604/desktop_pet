#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SVG → ESP32 1-bit 位图 C 数组 转换脚本
========================================
读取 image.svg,用 cairosvg 渲染 (完美抗锯齿),
再二值化为 1-bit 位图,PetDisplay::drawSleepyCat() 直接画黑点

数据量: 320×216 / 8 = 8640 bytes ≈ 8.4 KB PROGMEM (vs RGB565 135KB)
渲染:   ~2213 个黑点 × drawPixel ≈ 6ms/帧

用法:
  python svg2catpath.py            # 默认读 image.svg → cat_bitmap.h
"""

import sys
import os

DEFAULT_INPUT  = "image.svg"
DEFAULT_OUTPUT = "ESP32/src/desktop_pet/cat_bitmap.h"

# 屏幕参数 (留 24px 顶部状态栏,猫区 320×216)
SCREEN_W      = 320
SCREEN_H      = 240
STATUSBAR_H   = 24
CAT_W         = SCREEN_W                # 320
CAT_H         = SCREEN_H - STATUSBAR_H  # 216

# 渲染分辨率(2x 超采样再下采样,保留更多抗锯齿细节)
RENDER_SCALE  = 2

# 二值化阈值 (PIL 灰度 0-255,< threshold = 黑)
THRESHOLD     = 128

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

def generate_header(bmp_bytes, w, h, output_path):
    row_bytes = (w + 7) // 8
    n_bytes = len(bmp_bytes)
    assert n_bytes == row_bytes * h

    # 统计黑点数
    n_black = sum(8 - bin(b).count('1') for b in bmp_bytes)

    lines = []
    lines.append('/* ============================================================')
    lines.append(' * 自动生成 from image.svg,不要手改')
    lines.append(' * 生成命令: python svg2catpath.py')
    lines.append(f' * 尺寸: {w} x {h}  格式: 1-bit MSB-first')
    lines.append(f' * 渲染缩放: {RENDER_SCALE}x 超采样 + LANCZOS 下采样 + 阈值 {THRESHOLD}')
    lines.append(f' * 黑像素: {n_black} ({100*n_black/(w*h):.1f}%) 数据: {n_bytes} bytes ({n_bytes/1024:.1f} KB)')
    lines.append(' * 渲染: PetDisplay::drawSleepyCat() 扫字节,bit=0 调 drawPixel')
    lines.append(' * ============================================================ */')
    lines.append('')
    lines.append('#pragma once')
    lines.append('#include <Arduino.h>')
    lines.append('')
    lines.append(f'#define CAT_BMP_W         {w}')
    lines.append(f'#define CAT_BMP_H         {h}')
    lines.append(f'#define CAT_BMP_ROW_BYTES {row_bytes}')
    lines.append(f'#define CAT_BMP_BYTES     {n_bytes}')
    lines.append('')
    lines.append('/* MSB-first 1-bit: 0=猫身黑  1=背景白 (透明,不画) */')
    lines.append('static const uint8_t cat_bitmap[CAT_BMP_BYTES] PROGMEM = {')

    bytes_per_line = 16
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

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))

    return n_bytes

def main():
    input_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_INPUT
    output_path = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_OUTPUT

    if not os.path.exists(input_path):
        print(f"[错误] 找不到 {input_path}")
        sys.exit(1)

    print(f"[1/4] 检查依赖 ...")
    ensure_deps()

    print(f"[2/4] cairosvg 渲染 {input_path} → {CAT_W*RENDER_SCALE}x{CAT_H*RENDER_SCALE} ...")
    img = render_svg_to_pil(input_path, CAT_W, CAT_H)

    print(f"[3/4] 二值化 (阈值 {THRESHOLD}) ...")
    bmp = to_1bit_bytes(img, THRESHOLD)

    print(f"[4/4] 生成 {output_path} ...")
    n = generate_header(bmp, CAT_W, CAT_H, output_path)
    print(f"      OK {n} 字节 ({n/1024:.1f} KB) PROGMEM")

    print(f"\n[完成] PetDisplay::drawSleepyCat() 用 pgm_read_byte 扫位,bit=0 调 drawPixel")

if __name__ == "__main__":
    main()
