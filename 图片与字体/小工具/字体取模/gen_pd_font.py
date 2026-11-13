#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成 PD 检测 APP 专用字库 pd_text（16x16，FontInfo 格式：无文件头，
按字符表顺序逐字形存放 16*16 字节灰度位图，行优先）。

字符集：可打印 ASCII + PdTask.c 用到的全部汉字。
用法：  python gen_pd_font.py
输出：  图片与字体/font/pd_text.bin
        以及可直接粘贴到 font.c 的映射表与 FontInfo
"""
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ASSETS = os.path.dirname(os.path.dirname(HERE))          # 图片与字体/
FONT_DIR = os.path.join(ASSETS, "font")

CELL_W = 16
CELL_H = 16

CJK = "检测在线离电压流率请求旋转选择返回未握手请接快充适配器协商中成功不支持失败查连接，"

def pick(paths):
    for p in paths:
        if os.path.isfile(p):
            return p
    raise SystemExit("找不到字体")

TTF = pick(["C:/Windows/Fonts/msyh.ttc",
            os.path.join(ASSETS, "字体", "AiDianFengYaHei.ttf")])

chars = [chr(c) for c in range(0x20, 0x7F)]
for ch in CJK:
    if ch not in chars:
        chars.append(ch)

size = 13
font = ImageFont.truetype(TTF, size)
asc, desc = font.getmetrics()
# 汉字没有下伸部，只按 ascent 居中即可；否则 ascent+descent 一超格就要把字号降到 11，笔画糊成一团
top = max(0, (CELL_H - asc) // 2 - 1)

blobs = []
for ch in chars:
    img = Image.new("L", (CELL_W, CELL_H), 0)
    d = ImageDraw.Draw(img)
    bbox = d.textbbox((0, 0), ch, font=font, anchor="la")
    gw = bbox[2] - bbox[0]
    x = (CELL_W - gw) // 2 - bbox[0]
    d.text((x, top), ch, font=font, fill=255, anchor="la")
    blobs.append(img.tobytes())

out = os.path.join(FONT_DIR, "pd_text.bin")
with open(out, "wb") as fp:
    fp.write(b"".join(blobs))

print("font size %d, %d glyphs, %d bytes -> %s" % (size, len(chars), len(blobs) * CELL_W * CELL_H, out))

# C 侧映射表
rows = []
row = []
for ch in chars:
    b = ch.encode("utf-8")
    b = b + b"\x00" * (4 - len(b))
    row.append("{0x%02X,0x%02X,0x%02X,0x%02X}" % (b[0], b[1], b[2], b[3]))
    if len(row) == 4:
        rows.append("    " + ", ".join(row) + ",")
        row = []
if row:
    rows.append("    " + ", ".join(row) + ",")

print("")
print("const uint8_t font_map_pd_text[][4] = {")
print("\n".join(rows))
print("};")
print("")
print("const FontInfo pd_text = {")
print("    .font_width  = %d," % CELL_W)
print("    .font_height = %d," % CELL_H)
print("    .font_num    = %d," % len(chars))
print("    .path        = \"0:/font/pd_text.bin\",")
print("    .font_map    = font_map_pd_text")
print("};")
