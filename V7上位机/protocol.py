# -*- coding: utf-8 -*-
"""
上位机协议层：帧格式、CRC16、命令码、状态码。

与固件 UserCode/Task/APP/SerialCoreTask/SerialProto.c 一一对应：

    A5 5A | LEN_L LEN_H | CMD | SEQ | DATA[LEN] | CRC_L CRC_H

    LEN   : 负载长度（0 ~ 512，小端）
    CRC16 : CCITT-FALSE（poly 0x1021，初值 0xFFFF），覆盖 LEN ... DATA 末尾
    应答  : CMD / SEQ 原样返回，DATA[0] = 状态码，DATA[1..] = 数据
"""
from __future__ import annotations

import struct
import zlib

# ---------------------------------------------------------------- 帧常量
SOF0 = 0xA5
SOF1 = 0x5A
PROTO_VER = 2
HDR_SIZE = 6
MAX_PAYLOAD = 512
FS_CHUNK = 432          # 单次文件读写数据量（固件 FS_CHUNK_MAX）
OTA_CHUNK = 432         # 单次升级数据传输量（与 FS 相同，留足帧头空间）

# ---------------------------------------------------------------- 升级包格式（与固件 boot.h 一致）
OTA_HEAD_SIZE = 80
OTA_MAGIC = b"XMK7OTA1"
OTA_BOARD = "XM_Power_Kit_V7"
OTA_APP_ADDR = 0x08010000
OTA_APP_MAX = 448 * 1024

# 偏移不一致时设备回 ST_E_PARAM + 期望偏移，从这里续传
class OtaResync(Exception):
    def __init__(self, offset: int):
        self.offset = offset
        super().__init__("设备要求从偏移 %d 续传" % offset)

# ---------------------------------------------------------------- 命令码
CMD_SYS_INFO = 0x01
CMD_SYS_REBOOT = 0x02
CMD_SYS_BT_BAUD = 0x03

CMD_PARAM_GET = 0x10
CMD_PARAM_SET = 0x11

CMD_FS_LIST = 0x20
CMD_FS_READ = 0x21
CMD_FS_WRITE = 0x22
CMD_FS_DELETE = 0x23
CMD_FS_RENAME = 0x24
CMD_FS_INFO = 0x25
CMD_FS_MKDIR = 0x26
CMD_FS_STAT = 0x27
CMD_FS_SYNC = 0x28

CMD_OTA_INFO = 0x30
CMD_OTA_BEGIN = 0x31
CMD_OTA_DATA = 0x32
CMD_OTA_COMMIT = 0x33
CMD_OTA_ABORT = 0x34
CMD_OTA_ROLLBACK = 0x35

CMD_NAMES = {
    CMD_SYS_INFO: "SYS_INFO",
    CMD_SYS_REBOOT: "SYS_REBOOT",
    CMD_SYS_BT_BAUD: "SYS_BT_BAUD",
    CMD_PARAM_GET: "PARAM_GET",
    CMD_PARAM_SET: "PARAM_SET",
    CMD_FS_LIST: "FS_LIST",
    CMD_FS_READ: "FS_READ",
    CMD_FS_WRITE: "FS_WRITE",
    CMD_FS_DELETE: "FS_DELETE",
    CMD_FS_RENAME: "FS_RENAME",
    CMD_FS_INFO: "FS_INFO",
    CMD_FS_MKDIR: "FS_MKDIR",
    CMD_FS_STAT: "FS_STAT",
    CMD_FS_SYNC: "FS_SYNC",
    CMD_OTA_INFO: "OTA_INFO",
    CMD_OTA_BEGIN: "OTA_BEGIN",
    CMD_OTA_DATA: "OTA_DATA",
    CMD_OTA_COMMIT: "OTA_COMMIT",
    CMD_OTA_ABORT: "OTA_ABORT",
    CMD_OTA_ROLLBACK: "OTA_ROLLBACK",
}

# ---------------------------------------------------------------- 状态码
ST_OK = 0
ST_E_PARAM = 1
ST_E_FS = 2
ST_E_NOTFOUND = 3
ST_E_TOOBIG = 4
ST_E_BUSY = 5
ST_E_UNSUPPORTED = 6
ST_E_CRC = 7

STATUS_TEXT = {
    ST_OK: "成功",
    ST_E_PARAM: "参数错误",
    ST_E_FS: "文件系统错误",
    ST_E_NOTFOUND: "文件不存在",
    ST_E_TOOBIG: "数据过长",
    ST_E_BUSY: "设备忙",
    ST_E_UNSUPPORTED: "不支持的命令",
    ST_E_CRC: "校验失败",
}

# FRESULT -> 文本（FRESULT 为固件 FATFS 返回值）
FRESULT_TEXT = {
    0: "OK",
    1: "磁盘错误",
    2: "内部错误",
    3: "未就绪",
    4: "文件不存在",
    5: "路径不存在",
    6: "路径无效",
    7: "名称无效",
    8: "写保护",
    9: "介质无效",
    10: "无空闲簇",
    11: "根目录已满",
    12: "格式化未完成",
    13: "卷已挂载",
    14: "卷未挂载",
    15: "打开文件过多",
    16: "文件已存在",
    17: "文件被占用",
    18: "名称为空",
    19: "文件过大",
}

# ---------------------------------------------------------------- CRC16
def _make_crc_table():
    table = []
    for i in range(256):
        crc = i << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if (crc & 0x8000) else ((crc << 1) & 0xFFFF)
        table.append(crc)
    return table


_CRC_TABLE = _make_crc_table()


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc = ((crc << 8) & 0xFFFF) ^ _CRC_TABLE[((crc >> 8) ^ b) & 0xFF]
    return crc


def build_frame(cmd: int, seq: int, payload: bytes = b"") -> bytes:
    """组一帧：A5 5A LEN(2) CMD SEQ PAYLOAD CRC(2)"""
    if len(payload) > MAX_PAYLOAD:
        raise ValueError("payload 过长")
    body = struct.pack("<HBB", len(payload), cmd & 0xFF, seq & 0xFF) + payload
    return bytes((SOF0, SOF1)) + body + struct.pack("<H", crc16(body))


class FrameParser:
    """字节流 -> 完整帧。与固件里的重组算法等价：逐字节找帧头，长度/校验非法就丢 1 字节重同步。"""

    def __init__(self) -> None:
        self._buf = bytearray()

    def reset(self) -> None:
        self._buf.clear()

    def feed(self, data: bytes):
        """喂入原始字节，返回本次解析出的完整帧列表 [(cmd, seq, payload), ...]"""
        frames = []
        if not data:
            return frames
        self._buf.extend(data)
        if len(self._buf) > 4096:          # 防御：异常流不要无限增长
            del self._buf[:-1024]

        while True:
            if len(self._buf) < 2:
                return frames
            i = 0
            while i + 1 < len(self._buf):
                if self._buf[i] == SOF0 and self._buf[i + 1] == SOF1:
                    break
                i += 1
            if i + 1 >= len(self._buf):
                if self._buf[-1] == SOF0:
                    self._buf = bytearray([SOF0])
                else:
                    self._buf.clear()
                return frames
            if i:
                del self._buf[:i]

            if len(self._buf) < HDR_SIZE:
                return frames
            length = self._buf[2] | (self._buf[3] << 8)
            if length > MAX_PAYLOAD:
                del self._buf[:1]
                continue
            total = HDR_SIZE + length + 2
            if len(self._buf) < total:
                return frames
            calc = crc16(bytes(self._buf[2:2 + 4 + length]))
            recv = self._buf[total - 2] | (self._buf[total - 1] << 8)
            if calc != recv:
                del self._buf[:1]
                continue
            cmd = self._buf[4]
            seq = self._buf[5]
            payload = bytes(self._buf[HDR_SIZE:HDR_SIZE + length])
            del self._buf[:total]
            frames.append((cmd, seq, payload))


# ---------------------------------------------------------------- CRC32（升级包用）
def crc32(data: bytes, crc: int = 0) -> int:
    """标准 CRC-32（poly 0xEDB88320，初值 0xFFFFFFFF，结果取反），与固件 Boot_Crc32 一致。
    支持增量：crc32(b, crc32(a)) == crc32(a + b)。"""
    return zlib.crc32(data, crc) & 0xFFFFFFFF


class DeviceError(Exception):
    """设备返回了非 OK 状态码"""

    def __init__(self, status: int, detail: str = ""):
        self.status = status
        text = STATUS_TEXT.get(status, "未知错误(0x%02X)" % status)
        if detail:
            text = "%s：%s" % (text, detail)
        super().__init__(text)


class LinkError(Exception):
    """链路异常：未连接、超时、USB 断开等"""
