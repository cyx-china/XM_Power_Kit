# -*- coding: utf-8 -*-
"""
生成 bootloader 的点阵字库（BFNT 格式，放到设备外部 Flash 的 0:/font/boot.bin）。

字符集自动从 Bootloader/boot_ui.c 里的字符串字面量提取，改文案后重新跑一次即可。

用法：
    python gen_boot_font.py
输出：
    Bootloader/boot_font.bin
"""
import math
import os
import re
import struct

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.join(HERE, "..")
OUT = os.path.join(HERE, "..", "boot_font.bin")

FONT_CANDIDATES = [
    "C:/Windows/Fonts/simhei.ttf",   # 黑体：小字号下笔画清晰
    "C:/Windows/Fonts/msyh.ttc",
    "C:/Windows/Fonts/simsun.ttc",
]
FONT_SIZE = 15

STRING_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')


def unescape(text):
    """只处理常见的 C 转义，中文原样保留。"""
    mapping = {"n": "\n", "t": "\t", "r": "\r", '"': '"', "'": "'", "\\": "\\", "0": "\0"}
    out = []
    i = 0
    while i < len(text):
        ch = text[i]
        if ch == "\\" and (i + 1) < len(text):
            out.append(mapping.get(text[i + 1], text[i + 1]))
            i += 2
            continue
        out.append(ch)
        i += 1
    return "".join(out)


def collect_chars():
    # 扫描 bootloader 全部 .c（文案可能在 boot_ui.c / boot_selftest.c / boot_main.c）
    source = ""
    for name in sorted(os.listdir(SRC_DIR)):
        if name.endswith(".c"):
            with open(os.path.join(SRC_DIR, name), "r", encoding="utf-8") as fp:
                source += fp.read() + "\n"

    # 运行时会拼出来的字符（版本串/大小/CRC/百分比/倒计时）不在源码里，直接铺满可打印 ASCII
    chars = set(chr(c) for c in range(0x20, 0x7F))
    for raw in STRING_RE.findall(source):
        try:
            text = unescape(raw)
        except Exception:
            text = raw
        for ch in text:
            if ch in "\r\n\t":
                continue
            chars.add(ch)
    return sorted(chars)


def load_font():
    for path in FONT_CANDIDATES:
        if os.path.exists(path):
            return ImageFont.truetype(path, FONT_SIZE)
    raise SystemExit("找不到可用的中文字体")


def main():
    font = load_font()
    chars = collect_chars()

    # 先量一遍，确定统一的字高（以 ascender 线为公共原点，相对位置才对）
    metrics = []
    glyph_h = 0
    for ch in chars:
        x0, y0, x1, y1 = font.getbbox(ch, anchor="la")
        adv = int(round(font.getlength(ch)))
        width = int(math.ceil(x1))
        if width < 0:
            width = 0
        glyph_h = max(glyph_h, int(math.ceil(y1)))
        metrics.append((ch, width, max(adv, width), x0, y0))

    glyph_h = max(glyph_h, 1)

    glyphs = []
    for ch, width, adv, x0, y0 in metrics:
        if width == 0:
            glyphs.append((ord(ch), 0, adv, b""))
            continue
        img = Image.new("1", (width, glyph_h), 0)
        draw = ImageDraw.Draw(img)
        draw.text((0, 0), ch, font=font, fill=1, anchor="la")
        stride = (width + 7) // 8
        data = bytearray(stride * glyph_h)
        for r in range(glyph_h):
            for c in range(width):
                if img.getpixel((c, r)):
                    data[r * stride + (c >> 3)] |= 0x80 >> (c & 7)
        glyphs.append((ord(ch), width, adv, bytes(data)))

    count = len(glyphs)
    index_off = 16
    data_off = index_off + count * 8

    # 头 16 字节：magic(4) ver(1) glyph_h(1) flags(2) count(4) index_off(4)
    header = b"BFNT" + bytes([1, glyph_h, 0, 0]) + struct.pack("<II", count, index_off)
    index = bytearray()
    blob = bytearray()
    for cp, width, adv, data in glyphs:
        if cp > 0xFFFF:
            continue
        index += struct.pack("<HBB I".replace(" ", ""), cp, width & 0xFF, adv & 0xFF,
                             data_off + len(blob))
        blob += data

    out = header + bytes(index) + bytes(blob)
    with open(OUT, "wb") as fp:
        fp.write(out)

    print("字符数 %d，字高 %d，文件 %d 字节 -> %s" % (count, glyph_h, len(out), OUT))
    print("样本：" + "".join(chars[:40]))


if __name__ == "__main__":
    main()
