#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
验证 cat_bitmaps.h: 解析 C 数组,反渲成 PNG 看效果
不依赖 ESP32,纯 Python 反渲验证位图正确性

支持 8 心情 + 4 动作 = 12 张位图,生成:
  - cat_pic_preview/{mood_xxx,act_xxx}.png  单图
  - cat_pic_preview/_all_moods.png  心情拼图 (4x2)
  - cat_pic_preview/_all_actions.png 动作拼图 (4x1)
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
    """解析 cat_bitmaps.h,返回 (bitmaps_dict, mood_order, act_order)"""
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    # 找每个数组 (心情 mood_xxx 和动作 act_xxx 都被收集)
    pattern = re.compile(
        r'cat_bitmap_(\w+)\[CAT_BMP_BYTES\]\s*PROGMEM\s*=\s*\{([^}]+)\}',
        re.DOTALL
    )
    bmaps = {}
    for m in pattern.finditer(content):
        name = m.group(1)
        body = m.group(2)
        bytes_list = [int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', body)]
        bmaps[name] = bytes_list

    # 心情查表顺序
    mood_match = re.search(
        r'cat_bitmaps_mood\[CAT_BMP_MOOD_COUNT\]\s*PROGMEM\s*=\s*\{([^}]+)\}',
        content, re.DOTALL
    )
    mood_order = []
    if mood_match:
        mood_order = re.findall(r'cat_bitmap_mood_(\w+)', mood_match.group(1))

    # 动作查表顺序
    act_match = re.search(
        r'cat_bitmaps_act\[CAT_BMP_ACT_COUNT\]\s*PROGMEM\s*=\s*\{([^}]+)\}',
        content, re.DOTALL
    )
    act_order = []
    if act_match:
        act_order = re.findall(r'cat_bitmap_act_(\w+)', act_match.group(1))

    return bmaps, mood_order, act_order

def bmp_to_pil(bmp_bytes):
    """1-bit MSB-first bytes → PIL Image (1 mode)"""
    img = Image.new('1', (W, H), 1)  # 1=白
    px = img.load()
    for y in range(H):
        row_start = y * ROW_BYTES
        for xb in range(ROW_BYTES):
            byte = bmp_bytes[row_start + xb]
            for bit in range(7, -1, -1):
                x = xb * 8 + (7 - bit)
                if x >= W:
                    break
                if ((byte >> bit) & 1) == 0:
                    px[x, y] = 0
    return img

def bmp_to_png(bmp_bytes, out_path):
    img = bmp_to_pil(bmp_bytes)
    img.save(out_path)
    n_black = sum(8 - bin(b).count('1') for b in bmp_bytes)
    return n_black

def make_contact_sheet(bmaps, order, out_path, title):
    """把所有 order 里的位图拼成一张大图便于对比"""
    from PIL import ImageDraw, ImageFont
    cell_w = W // 2
    cell_h = H // 2
    cols = 4
    rows = (len(order) + cols - 1) // cols
    sheet = Image.new('RGB', (cell_w * cols + 10 * (cols + 1), cell_h * rows + 30 * (rows + 1)), (250, 247, 242))
    draw = ImageDraw.Draw(sheet)
    draw.text((10, 4), title, fill=(30, 30, 30))
    for i, name in enumerate(order):
        col = i % cols
        row = i // cols
        x = 10 + col * (cell_w + 10)
        y = 30 + row * (cell_h + 30)
        if name in bmaps:
            sub = bmp_to_pil(bmaps[name]).resize((cell_w, cell_h), Image.NEAREST).convert('RGB')
            sheet.paste(sub, (x, y))
            draw.text((x + 4, y + cell_h + 2), name.upper(), fill=(50, 50, 50))
    sheet.save(out_path)

def main():
    if not os.path.exists(HEADER):
        print(f"[错误] 找不到 {HEADER},先跑 python svg2catpath.py")
        return

    os.makedirs(OUT_DIR, exist_ok=True)
    bmaps, mood_order, act_order = parse_header(HEADER)
    print(f"[1/3] 解析到 {len(bmaps)} 个位图")
    print(f"      心情 ({len(mood_order)}): {mood_order}")
    print(f"      动作 ({len(act_order)}): {act_order}")

    print(f"[2/3] 反渲单图到 {OUT_DIR}/ ...")
    for name in mood_order:
        key = f"mood_{name}"   # 心情查表返回 "happy", 实际键是 "mood_happy"
        if key in bmaps:
            n = bmp_to_png(bmaps[key], f"{OUT_DIR}/{key}.png")
            print(f"  - {key:14s}: {n:5d} 黑点 ({100*n/(W*H):.1f}%)")
    for name in act_order:
        key = f"act_{name}"    # 动作查表返回 "eat", 实际键是 "act_eat"
        if key in bmaps:
            n = bmp_to_png(bmaps[key], f"{OUT_DIR}/{key}.png")
            print(f"  - {key:14s}: {n:5d} 黑点 ({100*n/(W*H):.1f}%)")

    print(f"[3/3] 生成拼图 ...")
    if mood_order:
        make_contact_sheet(bmaps, [f"mood_{n}" for n in mood_order],
                           f"{OUT_DIR}/_all_moods.png",
                           "MOODS (8): happy normal hungry sleepy angry sick excited love")
        print(f"  - {OUT_DIR}/_all_moods.png")
    if act_order:
        make_contact_sheet(bmaps, [f"act_{n}" for n in act_order],
                           f"{OUT_DIR}/_all_actions.png",
                           "ACTIONS (4): eat play vibration stroke")
        print(f"  - {OUT_DIR}/_all_actions.png")

if __name__ == "__main__":
    main()
