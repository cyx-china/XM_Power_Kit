# -*- coding: utf-8 -*-
"""生成 fft_twiddle.h：1024 点 FFT 的旋转因子表（N/2 = 512 项）。
表项 m 对应 e^(-j*2*pi*m/1024)，即 cos/sin 两个 float 数组。
用法：python gen_twiddle.py  （在 UserCode/Task/APP/DsoTask/FFT 目录下运行）
"""
import math
import os

N = 1024
HALF = N // 2 + 1     # 0..N/2，含 cos(pi) = -1 这一项（汉宁窗要用到）
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fft_twiddle.h")

HEAD = """/**
 * @file    fft_twiddle.h
 * @brief   1024 点基2 FFT 旋转因子表（自动生成，请勿手工修改）
 * @note    由 FFT/gen_twiddle.py 生成。表项 m 对应 e^(-j*2*pi*m/1024)，
 *          即 cos(2*pi*m/N)、sin(2*pi*m/N)，m 取值 0..N/2（共 N/2+1 项）。
 *          运行期蝶形按 stride = N/k 取表，不再调用 cosf/sinf。
 */
#ifndef __FFT_TWIDDLE_H
#define __FFT_TWIDDLE_H

#define FFT_TW_N {half}U

static const float FFT_TW_COS[FFT_TW_N] = {{
""".format(half=HALF)

MID = """};

static const float FFT_TW_SIN[FFT_TW_N] = {
"""

TAIL = """};

#endif
"""

def fmt(v):
    s = "%.9ff" % v
    return s

def block(name, func):
    items = []
    for m in range(HALF):
        items.append(fmt(func(2.0 * math.pi * m / N)))
    lines = []
    for i in range(0, HALF, 8):
        lines.append("    " + ", ".join(items[i:i + 8]) + ",")
    return "\n".join(lines) + "\n"

with open(OUT, "w", encoding="utf-8", newline="\n") as f:
    f.write(HEAD)
    f.write(block("cos", math.cos))
    f.write(MID)
    f.write(block("sin", math.sin))
    f.write(TAIL)

print("written", OUT)
print("cos[0]=%s cos[1]=%s sin[1]=%s cos[512]=%s" % (
    fmt(math.cos(0.0)), fmt(math.cos(2*math.pi/N)), fmt(math.sin(2*math.pi/N)),
    fmt(math.cos(math.pi * 512.0 / (N / 2.0)))))
