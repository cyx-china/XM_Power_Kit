#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成 bootloader 用的参数默认值镜像 Bootloader/Inc/boot_param_default.h

bootloader 在新机（或参数文件被删）时把这份 224 字节的默认值直接写成
0:/uparam.bin，这样用户第一次上电就有完整的配置文件，不需要等主程序去建。

数据来源（与应用完全一致，不重复维护）：
  * 顺序与长度：UserCode/Manager/UserDefineManage.c 的 param_desc_table[]
  * 默认值    ：同文件 UserParamDefault 的初始化列表
  * 存放规则  ：同文件 UserParam_ResetToDefault_NoLock()
                —— 224 字节，每参数固定 4 字节，2 字节类型只占低 2 字节，高 2 字节为 0

**修改 UserDefineManage.c 里的默认值后必须重新运行本脚本**：
    python Bootloader/tools/gen_param_default.py
"""
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(ROOT, "UserCode", "Manager", "UserDefineManage.c")
OUT = os.path.join(ROOT, "Bootloader", "Inc", "boot_param_default.h")


def read_text(path):
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        return f.read()


def parse_table(text):
    """param_desc_table：{len, &UserParamDefault.NAME, &UserParam.NAME}"""
    m = re.search(r"param_desc_table\s*\[[^\]]*\]\s*=\s*\{(.*?)\n\};", text, re.S)
    if not m:
        sys.exit("找不到 param_desc_table")
    items = re.findall(
        r"\{\s*(\d+)\s*,\s*&UserParamDefault\.(\w+)\s*,\s*&UserParam\.(\w+)\s*\}", m.group(1))
    return [(int(n), name) for n, name, _ in items]


def parse_defaults(text):
    """UserParamDefault 的指定化初始化列表 -> {NAME: 数值}"""
    m = re.search(r"UserParamDefault\s*=\s*\{(.*?)\n\};", text, re.S)
    if not m:
        sys.exit("找不到 UserParamDefault")
    out = {}
    for name, raw in re.findall(r"\.(\w+)\s*=\s*([^,\n]+)", m.group(1)):
        raw = raw.split("//")[0].strip()
        if raw.endswith("f") or raw.endswith("F"):
            out[name] = float(raw[:-1])
        elif "." in raw:
            out[name] = float(raw)
        else:
            out[name] = int(raw)
    return out


def main():
    text = read_text(SRC)
    table = parse_table(text)
    defaults = parse_defaults(text)

    blobs = []
    for length, name in table:
        if name not in defaults:
            sys.exit("参数 %s 在 UserParamDefault 里找不到默认值" % name)
        value = defaults[name]
        if length == 2:
            if isinstance(value, float):
                if value != int(value):
                    sys.exit("参数 %s 是 2 字节类型，默认值不能带小数" % name)
                value = int(value)
            blobs.append(struct.pack("<h" if value < 0 else "<H", value) + b"\x00\x00")
        elif length == 4:
            blobs.append(struct.pack("<f", float(value)))
        else:
            sys.exit("参数 %s 的长度 %d 不支持" % (name, length))

    data = b"".join(blobs)
    lines = []
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        lines.append("    " + ", ".join("0x%02X" % b for b in chunk) + ",")

    out = []
    out.append("/**")
    out.append(" ******************************************************************************")
    out.append(" * @file    boot_param_default.h")
    out.append(" * @brief   参数文件 0:/uparam.bin 的默认内容（由 tools/gen_param_default.py 生成，")
    out.append(" *          数据源 UserCode/Manager/UserDefineManage.c，请勿手改）")
    out.append(" *")
    out.append(" *          %d 个参数 × 4 字节 = %d 字节，2 字节类型只占低 2 字节。" % (len(table), len(data)))
    out.append(" *          修改默认值后重新运行：python Bootloader/tools/gen_param_default.py")
    out.append(" ******************************************************************************")
    out.append(" */")
    out.append("#ifndef BOOT_PARAM_DEFAULT_H")
    out.append("#define BOOT_PARAM_DEFAULT_H")
    out.append("")
    out.append("#include <stdint.h>")
    out.append("")
    out.append("#define BOOT_PARAM_COUNT    %dU" % len(table))
    out.append("#define BOOT_PARAM_BYTES    %dU" % len(data))
    out.append("")
    out.append("static const uint8_t boot_param_default[BOOT_PARAM_BYTES] = {")
    out.extend(lines)
    out.append("};")
    out.append("")
    out.append("#endif /* BOOT_PARAM_DEFAULT_H */")
    out.append("")

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out))

    print("参数个数 %d，镜像 %d 字节 -> %s" % (len(table), len(data), OUT))
    print("前 8 个参数:", ", ".join(n for _, n in table[:8]))


if __name__ == "__main__":
    main()