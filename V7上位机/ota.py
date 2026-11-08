# -*- coding: utf-8 -*-
"""升级包：头组装、固件版本识别、镜像校验。

包格式（80 字节头 + 裸镜像，小端），与固件 Bootloader/Inc/boot.h 完全一致：

    0   magic[8]        "XMK7OTA1"
    8   version[32]     固件版本串，如 "STM32F407VE.261108.Rel.0007"
    40  image_size      u32 镜像字节数
    44  image_crc       u32 镜像 CRC32
    48  head_crc        u32 覆盖 0..47 字节的 CRC32
    52  load_addr       u32 目标地址，固定 0x08008000
    56  build_time[8]   "YYYYMMDD"
    64  board[16]       机型串
"""
from __future__ import annotations

import os
import re
import struct
from datetime import datetime

from protocol import (
    OTA_APP_ADDR, OTA_APP_MAX, OTA_BOARD, OTA_HEAD_SIZE, OTA_MAGIC, crc32,
)

_VERSION_RE = re.compile(rb"STM32F407VE\.\d{6}\.[A-Za-z]+\.\d{4}")


def sniff_version(image: bytes) -> str:
    """从固件镜像里找出 SYS_VERSION_CODE（字符串常量就在 .rodata 里）"""
    m = _VERSION_RE.search(image)
    return m.group(0).decode("ascii") if m else ""


def build_head(version: str, image_size: int, image_crc: int,
               build_time: str | None = None, board: str = OTA_BOARD) -> bytes:
    """组装 80 字节包头"""
    if image_size <= 0 or image_size > OTA_APP_MAX:
        raise ValueError("镜像大小 %d 超出范围（上限 %d）" % (image_size, OTA_APP_MAX))
    if not version.strip():
        raise ValueError("版本串不能为空")
    if build_time is None:
        build_time = datetime.now().strftime("%Y%m%d")
    if len(build_time) != 8 or not build_time.isdigit():
        raise ValueError("构建日期必须是 8 位数字 YYYYMMDD")

    head = bytearray(OTA_HEAD_SIZE)
    head[0:8] = OTA_MAGIC
    vb = version.encode("utf-8")[:31]
    head[8:8 + len(vb)] = vb
    struct.pack_into("<I", head, 40, image_size)
    struct.pack_into("<I", head, 44, image_crc)
    struct.pack_into("<I", head, 52, OTA_APP_ADDR)
    tb = build_time.encode("ascii")[:8]
    head[56:56 + len(tb)] = tb
    bb = board.encode("ascii")[:15]
    head[64:64 + len(bb)] = bb
    struct.pack_into("<I", head, 48, crc32(bytes(head[0:48])))
    return bytes(head)


def parse_head(head: bytes) -> dict:
    """解析包头（失败抛 ValueError），用于本地试读或设备回传的包"""
    if len(head) < OTA_HEAD_SIZE:
        raise ValueError("包头长度不足")
    if head[0:8] != OTA_MAGIC:
        raise ValueError("不是升级包（magic 不符）")
    want = struct.unpack("<I", head[48:52])[0]
    if crc32(head[0:48]) != want:
        raise ValueError("包头校验失败")
    return {
        "version": head[8:40].split(b"\0")[0].decode("utf-8", "replace"),
        "image_size": struct.unpack("<I", head[40:44])[0],
        "image_crc": struct.unpack("<I", head[44:48])[0],
        "load_addr": struct.unpack("<I", head[52:56])[0],
        "build_time": head[56:64].split(b"\0")[0].decode("ascii", "replace"),
        "board": head[64:80].split(b"\0")[0].decode("ascii", "replace"),
    }


def load_image(path: str) -> bytes:
    """读镜像文件。如果传进来的是已经带包头的包，自动剥掉头。"""
    with open(path, "rb") as f:
        data = f.read()
    if len(data) > OTA_HEAD_SIZE and data[0:8] == OTA_MAGIC:
        info = parse_head(data[:OTA_HEAD_SIZE])
        body = data[OTA_HEAD_SIZE:OTA_HEAD_SIZE + info["image_size"]]
        if len(body) != info["image_size"]:
            raise ValueError("升级包长度与包头声明不符")
        if crc32(body) != info["image_crc"]:
            raise ValueError("升级包镜像 CRC 校验失败")
        return body
    return data


def make_package(bin_path: str, version: str | None = None) -> dict:
    """校验镜像并生成包头，返回 {head, image, version, size, crc, build_time}"""
    image = load_image(bin_path)
    if len(image) < 8:
        raise ValueError("镜像太小，不像是有效的固件")
    if len(image) > OTA_APP_MAX:
        raise ValueError("镜像 %d 字节，超过应用分区上限 %d 字节" % (len(image), OTA_APP_MAX))

    if version is None or not version.strip():
        version = sniff_version(image)
    if not version:
        raise ValueError("没能在镜像里找到版本号，请手动填写")

    image_crc = crc32(image)
    build_time = datetime.now().strftime("%Y%m%d")
    head = build_head(version, len(image), image_crc, build_time)
    return {
        "head": head,
        "image": image,
        "version": version,
        "size": len(image),
        "crc": image_crc,
        "build_time": build_time,
    }


def write_package(bin_path: str, out_path: str, version: str | None = None) -> dict:
    """把裸 .bin 打包成带头的 .xmkota（也可直接上传裸 .bin，设备只认头+镜像）"""
    pkg = make_package(bin_path, version)
    with open(out_path, "wb") as f:
        f.write(pkg["head"])
        f.write(pkg["image"])
    return pkg
