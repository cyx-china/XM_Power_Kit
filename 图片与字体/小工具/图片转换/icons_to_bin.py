#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
把 图片与字体/图标/*.png 批量转成自研图形库用的原始 RGB565 .bin
（无文件头，逐像素小端 RGB565，行优先；GFX_DrawImage 要求文件长度严格等于 w*h*2）

用法：  python icons_to_bin.py [--dir 图标] [--out images]
输出：  图片与字体/images/<name>.bin ，并打印可直接粘贴的 ImageInfo 声明
"""
import argparse
import os
import re
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # 图片与字体/

def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def convert(src, dst):
    img = Image.open(src).convert("RGB")
    w, h = img.size
    data = bytearray()
    px = img.load()
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            v = rgb565(r, g, b)
            data.append(v & 0xFF)
            data.append((v >> 8) & 0xFF)
    with open(dst, "wb") as f:
        f.write(data)
    return w, h, len(data)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", default=os.path.join(ROOT, "图标"))
    ap.add_argument("--out", default=os.path.join(ROOT, "images"))
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    names = sorted(f for f in os.listdir(args.dir) if f.lower().endswith(".png"))
    decls = []
    for name in names:
        src = os.path.join(args.dir, name)
        base = re.sub(r"[^a-zA-Z0-9_-]", "_", os.path.splitext(name)[0]).strip("_")
        w, h, size = convert(src, os.path.join(args.out, base + ".bin"))
        sym = "img_%s_%dx%d" % (base, w, h)
        decls.append('extern const ImageInfo %s;' % sym)
        print("%-14s -> %-28s %dx%d %d bytes" % (name, base + ".bin", w, h, size))

    print("\n/* exported from font/images header */")
    for d in decls:
        print(d)

if __name__ == "__main__":
    main()
