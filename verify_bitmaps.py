#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
验证 cat_bitmaps.h: 解析 C 数组,反渲成 PNG 看效果
不依赖 ESP32,纯 Python 反渲验证位图正确性
"""

import re
import os
from PIL import Image

HEADER = "ESP32/src/desktop_pet/cat_bitmaps.h"
OUT_DIR = "cat_pic_preview"
W = 320
H = 216
ROW_BYTES = 40

def parse_header(path):
    """解析 cat_bitmaps.h,返回 {mood_name: [bytes]}"""
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    # 找每个数组
    pattern = re.compile(
        r'cat_bitmap_(\w+)\[CAT_BMP_BYTES\]\s*PROGMEM\s*=\s*\{([^}]+)\}',
        re.DOTALL
    )
    bmaps = {}
    for m in pattern.finditer(content):
        name = m.group(1)
        body = m.group(2)
        # 提取所有 0xNN
        bytes_list = [int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', body)]
        bmaps[name] = bytes_list

    # 找查表顺序
    table_match = re.search(
        r'cat_bitmaps\[CAT_BMP_MOOD_COUNT\]\s*PROGMEM\s*=\s*\{([^}]+)\}',
        content, re.DOTALL
    )
    order = []
    if table_match:
        order = re.findall(r'cat_bitmap_(\w+)', table_match.group(1))

    return bmaps, order

def bmp_to_png(bmp_bytes, out_path):
    """1-bit MSB-first bytes → PNG"""
    img = Image.new('1', (W, H), 1)  # 1 = 白
    px = img.load()
    for y in range(H):
        row_start = y * ROW_BYTES
        for xb in range(ROW_BYTES):
            byte = bmp_bytes[row_start + xb]
            for bit in range(7, -1, -1):
                x = xb * 8 + (7 - bit)
                if x >= W:
                    break
                # 0 = 黑
                if ((byte >> bit) & 1) == 0:
                    px[x, y] = 0
    img.save(out_path)
    # 统计黑点数
    n_black = sum(8 - bin(b).count('1') for b in bmp_bytes)
    return n_black

def make_contact_sheet(bmaps, order, out_path):
    """把所有 mood 拼成一张大图便于对比"""
    cell_w = W // 2  # 缩放
    cell_h = H // 2
    cols = 4
    rows = (len(order) + cols - 1) // cols
    sheet = Image.new('RGB', (cell_w * cols + 10 * (cols + 1), cell_h * rows + 30 * (rows + 1)), (250, 247, 242))
    from PIL import ImageDraw, ImageFont
    draw = ImageDraw.Draw(sheet)
    for i, name in enumerate(order):
        col = i % cols
        row = i // cols
        x = 10 + col * (cell_w + 10)
        y = 30 + row * (cell_h + 30)
        if name in bmaps:
            # 1-bit → 缩放
            sub = Image.new('1', (W, H), 1)
            px = sub.load()
            for sy in range(H):
                row_start = sy * ROW_BYTES
                for sxb in range(ROW_BYTES):
                    byte = bmaps[name][row_start + sxb]
                    for bit in range(7, -1, -1):
                        sx = sxb * 8 + (7 - bit)
                        if sx >= W: break
                        if ((byte >> bit) & 1) == 0:
                            px[sx, sy] = 0
            sub = sub.resize((cell_w, cell_h), Image.NEAREST)
            # 转 RGB 才能贴
            sub_rgb = sub.convert('RGB')
            sheet.paste(sub_rgb, (x, y))
            draw.text((x + 4, y + cell_h + 2), name.upper(), fill=(50, 50, 50))
    sheet.save(out_path)

def main():
    if not os.path.exists(HEADER):
        print(f"[错误] 找不到 {HEADER},先跑 python svg2catpath.py")
        return

    os.makedirs(OUT_DIR, exist_ok=True)
    bmaps, order = parse_header(HEADER)
    print(f"[1/2] 解析到 {len(bmaps)} 个 mood: {list(bmaps.keys())}")
    print(f"      查表顺序: {order}")

    print(f"[2/2] 反渲到 {OUT_DIR}/ ...")
    for name in order:
        if name in bmaps:
            n_black = bmp_to_png(bmaps[name], f"{OUT_DIR}/{name}.png")
            pct = 100 * n_black / (W * H)
            print(f"  - {name:8s}: {n_black:5d} 黑点 ({pct:.1f}%)")

    contact_path = f"{OUT_DIR}/_all_moods.png"
    make_contact_sheet(bmaps, order, contact_path)
    print(f"  - 拼图: {contact_path} (4x2 缩放对比)")

if __name__ == "__main__":
    main()
