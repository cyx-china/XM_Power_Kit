#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成自研 GUI 用的外部字库（FontInfo 格式：无文件头，按字符表顺序逐字形存放
font_width*font_height 字节灰度位图，行优先），同时生成 C 侧映射表 ui_fonts.c。

字符集来源：扫描 UserCode/Gui/*.c 里的全部 C 字符串字面量 + 可打印 ASCII。

用法：python gen_ui_font.py [--preview]
输出：
  图片与字体/font/ui16x16.bin / ui_title.bin / ui_mono.bin
  UserCode/Drivers/TFT_LCD/fonts/ui_fonts.c
  可选：图片与字体/font/_preview_ui.png（预览）
"""
import argparse
import os
import re
import sys

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ASSETS = os.path.dirname(os.path.dirname(HERE))          # 图片与字体/
PROJECT = os.path.dirname(ASSETS)                         # 工程根目录
GUI_DIR = os.path.join(PROJECT, "UserCode", "Gui")
FONT_DIR = os.path.join(ASSETS, "font")
C_OUT = os.path.join(PROJECT, "UserCode", "Drivers", "TFT_LCD", "fonts", "ui_fonts.c")

# 字体候选（优先用系统雅黑，找不到时退回仓库里的中文字体）
CJK_CANDIDATES = [
    "C:/Windows/Fonts/msyh.ttc",
    os.path.join(ASSETS, "字体", "AiDianFengYaHei.ttf"),
]
TITLE_CANDIDATES = [
    os.path.join(ASSETS, "字体", "Acme-Regular.ttf"),
    "C:/Windows/Fonts/arial.ttf",
]
MONO_CANDIDATES = [
    os.path.join(ASSETS, "字体", "JetBrainsMono.ttf"),
    "C:/Windows/Fonts/consola.ttf",
]

ASCII = "".join(chr(c) for c in range(0x20, 0x7F))


def pick(candidates):
    for path in candidates:
        if os.path.isfile(path):
            return path
    raise SystemExit("找不到可用字体：%s" % candidates)


def collect_chars():
    chars = set(ASCII)
    for name in sorted(os.listdir(GUI_DIR)):
        if not name.endswith(".c"):
            continue
        text = open(os.path.join(GUI_DIR, name), encoding="utf-8").read()
        for lit in re.findall(r'"((?:[^"\\]|\\.)*)"', text):
            s = (lit.replace("\\n", "\n").replace('\\"', '"')
                    .replace("\\\\", "\\"))
            for ch in s:
                if ord(ch) >= 0x20:
                    chars.add(ch)
    return chars


def fit_size(ttf, cell_h, prefer):
    """挑一个 ascent+descent 不超过单元格高度、且尽量接近 prefer 的字号"""
    size = prefer
    while size > 6:
        font = ImageFont.truetype(ttf, size)
        ascent, descent = font.getmetrics()
        if ascent + descent <= cell_h:
            return size
        size -= 1
    return 6


def build_font(ttf, cell_w, cell_h, chars, prefer_size):
    size = fit_size(ttf, cell_h, prefer_size)
    font = ImageFont.truetype(ttf, size)
    ascent, descent = font.getmetrics()
    top = max(0, (cell_h - (ascent + descent)) // 2)
    ordered = sorted(chars)
    blobs = []
    for ch in ordered:
        img = Image.new("L", (cell_w, cell_h), 0)
        draw = ImageDraw.Draw(img)
        bbox = draw.textbbox((0, 0), ch, font=font, anchor="la")
        gw = bbox[2] - bbox[0]
        x = (cell_w - gw) // 2 - bbox[0]
        draw.text((x, top), ch, font=font, fill=255, anchor="la")
        blobs.append(img.tobytes())
    return ordered, b"".join(blobs), size


def c_map(name, chars):
    lines = []
    lines.append("static const uint8_t font_map_%s[][4] = {" % name)
    row = []
    for ch in chars:
        b = ch.encode("utf-8")
        if len(b) > 4:
            continue
        b = b + b"\x00" * (4 - len(b))
        row.append("0x%02X,0x%02X,0x%02X,0x%02X" % (b[0], b[1], b[2], b[3]))
        if len(row) == 4:
            lines.append("    " + ", ".join(row) + ",")
            row = []
    if row:
        lines.append("    " + ", ".join(row) + ",")
    lines.append("};")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview", action="store_true")
    args = ap.parse_args()

    os.makedirs(FONT_DIR, exist_ok=True)
    chars = collect_chars()

    specs = [
        ("ui_text", pick(CJK_CANDIDATES), 16, 20, 15, chars),
        ("ui_title", pick(TITLE_CANDIDATES), 16, 28, 21, set("XMPowerKit ")),
        ("ui_mono", pick(MONO_CANDIDATES), 9, 16, 11, set(ASCII)),
    ]

    c_parts = ['#include "ui_fonts.h"', ""]
    for sym, ttf, cw, chh, prefer, charset in specs:
        ordered, data, size = build_font(ttf, cw, chh, charset, prefer)
        out = os.path.join(FONT_DIR, sym + ".bin")
        with open(out, "wb") as f:
            f.write(data)
        print("%-9s %-34s cell %dx%d size %d  %3d chars  %5d bytes" %
              (sym, os.path.basename(ttf), cw, chh, size, len(ordered), len(data)))
        c_parts.append(c_map(sym, ordered))
        c_parts.append("const FontInfo %s = { .font_width = %d, .font_height = %d, .font_num = %d,"
                       " .path = \"0:/font/%s.bin\", .font_map = font_map_%s };"
                       % (sym, cw, chh, len(ordered), sym, sym))
        c_parts.append("")
        if args.preview:
            preview(ttf, size, cw, chh)

    with open(C_OUT, "w", encoding="utf-8", newline="\r\n") as f:
        f.write("\n".join(c_parts))
    print("写出 C 映射表：", os.path.relpath(C_OUT, PROJECT))


def preview(ttf, size, cw, chh):
    """在字体目录下拼一张样张，方便肉眼检查字形"""
    font = ImageFont.truetype(ttf, size)
    samples = ["XM Power Kit", "0123456789 .%℃", "电源 示波器 信号源 万用表", "设置 关于 校准 传感器"]
    scale = 3
    w = 420 * scale
    h = (len(samples) * (chh + 6) + 10) * scale
    img = Image.new("RGB", (w, h), (10, 14, 24))
    d = ImageDraw.Draw(img)
    y = 6 * scale
    for line in samples:
        x = 6 * scale
        for ch in line:
            if ch == " ":
                x += (cw + 2) * scale
                continue
            g = Image.new("L", (cw, chh), 0)
            gd = ImageDraw.Draw(g)
            bbox = gd.textbbox((0, 0), ch, font=font, anchor="la")
            gw = bbox[2] - bbox[0]
            gd.text(((cw - gw) // 2 - bbox[0], 0), ch, font=font, fill=255, anchor="la")
            g = g.resize((cw * scale, chh * scale), Image.NEAREST)
            img.paste(Image.merge("RGB", (g.point(lambda v: v), g.point(lambda v: v),
                                          g.point(lambda v: v))), (x, y))
            x += (cw + 2) * scale
        y += (chh + 6) * scale
    out = os.path.join(FONT_DIR, "_preview_" + os.path.basename(ttf).split(".")[0] + ".png")
    img.save(out)
    print("预览：", out)


if __name__ == "__main__":
    main()
