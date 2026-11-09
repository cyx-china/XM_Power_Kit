# -*- coding: utf-8 -*-
"""生成上位机图标（深色圆角方块 + 白色闪电），输出 icon.ico / icon.png。

用法：python make_icon.py
依赖：Pillow
"""
from __future__ import annotations

import os

from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ICO_SIZES = [16, 24, 32, 48, 64, 128, 256]
SMALL_SIZES = {16, 24, 32}       # 小尺寸用加粗闪电、去掉辉光，任务栏才看得清

# 深色背景（上浅下深）与闪电高光
BG_TOP = (12, 19, 33)
BG_BOTTOM = (32, 47, 68)
ACCENT = (56, 189, 248)      # 青色，仅用于辉光
BOLT = (248, 250, 252)

# 闪电轮廓（1024 画布坐标）
BOLT_POINTS = [(612, 60), (196, 596), (452, 596), (352, 976), (784, 380), (528, 380)]

_CACHE = {}


def _vertical_gradient(size: int, top, bottom) -> Image.Image:
    img = Image.new("RGB", (size, size))
    draw = ImageDraw.Draw(img)
    for y in range(size):
        t = y / max(1, size - 1)
        draw.line([(0, y), (size, y)],
                  fill=(int(top[0] + (bottom[0] - top[0]) * t),
                        int(top[1] + (bottom[1] - top[1]) * t),
                        int(top[2] + (bottom[2] - top[2]) * t)))
    return img


def build(size: int, small: bool = False) -> Image.Image:
    """渲染一幅 size×size 的图标；small=True 时不画辉光、闪电加粗"""
    base = 512
    canvas = Image.new("RGBA", (base, base), (0, 0, 0, 0))

    radius = int(base * 0.20)
    mask = Image.new("L", (base, base), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, base - 1, base - 1], radius=radius, fill=255)

    body = _vertical_gradient(base, BG_TOP, BG_BOTTOM).convert("RGBA")

    gloss = Image.new("RGBA", (base, base), (0, 0, 0, 0))
    gd = ImageDraw.Draw(gloss)
    gd.ellipse([-base * 0.35, -base * 0.75, base * 1.35, base * 0.42],
               fill=(ACCENT[0], ACCENT[1], ACCENT[2], 46 if not small else 30))
    body = Image.alpha_composite(body, gloss)
    canvas.paste(body, (0, 0), mask)

    # 闪电图层（1024 坐标 → 512）
    bolt = Image.new("L", (base, base), 0)
    ImageDraw.Draw(bolt).polygon([(x // 2, y // 2) for x, y in BOLT_POINTS], fill=255)
    if small:
        bolt = bolt.filter(ImageFilter.MaxFilter(9))       # 加粗，16 px 下才成块

    if not small:
        glow = bolt.filter(ImageFilter.GaussianBlur(base * 0.05)).point(lambda v: int(v * 0.9))
        tinted = Image.new("RGBA", (base, base), ACCENT + (0,))
        tinted.putalpha(glow)
        canvas = Image.alpha_composite(canvas, tinted)

    solid = Image.new("RGBA", (base, base), BOLT + (0,))
    solid.putalpha(bolt)
    canvas = Image.alpha_composite(canvas, solid)

    out = Image.new("RGBA", (base, base), (0, 0, 0, 0))
    out.paste(canvas, (0, 0), mask)
    return out.resize((size, size), Image.LANCZOS)


def _variants() -> dict:
    if not _CACHE:
        big = build(512)
        small = build(512, small=True)
        for s in ICO_SIZES:
            _CACHE[s] = small if s in SMALL_SIZES else big.resize((s, s), Image.LANCZOS)
    return _CACHE


def main() -> None:
    variants = _variants()
    ico_path = os.path.join(HERE, "icon.ico")
    png_path = os.path.join(HERE, "icon.png")
    base = variants[256]
    base.save(ico_path, sizes=[(s, s) for s in ICO_SIZES],
              append_images=[variants[s] for s in ICO_SIZES if s != 256])
    variants[256].resize((512, 512), Image.LANCZOS).save(png_path)
    print("写入", ico_path, os.path.getsize(ico_path), "字节")
    print("写入", png_path, os.path.getsize(png_path), "字节")


if __name__ == "__main__":
    main()
