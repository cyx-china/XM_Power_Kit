# -*- coding: utf-8 -*-
"""设备访问层：串口收发 + 请求/应答 + 高层操作（参数、文件）。"""
from __future__ import annotations

import os
import struct
import time

import serial
import serial.tools.list_ports

import params as P
from protocol import (
    CMD_FS_DELETE, CMD_FS_INFO, CMD_FS_LIST, CMD_FS_MKDIR, CMD_FS_READ,
    CMD_FS_RENAME, CMD_FS_STAT, CMD_FS_SYNC, CMD_FS_WRITE, CMD_PARAM_GET,
    CMD_PARAM_SET, CMD_SYS_BT_BAUD, CMD_SYS_INFO, CMD_SYS_REBOOT, CMD_NAMES,
    FS_CHUNK, FrameParser, DeviceError, LinkError, build_frame, STATUS_TEXT,
)

FS_ATTR_DIR = 0x10


def list_ports():
    """返回 [(设备名, 描述), ...]，USB CDC 与蓝牙 SPP 在这里都表现成 COM 口"""
    out = []
    for p in serial.tools.list_ports.comports():
        desc = (p.description or "").strip()
        if len(desc) > 34:
            desc = desc[:34] + "…"
        out.append((p.device, desc))
    return out


class Device:
    def __init__(self) -> None:
        self._ser = None
        self._parser = FrameParser()
        self._seq = 0
        self._pending_frames = []
        self.log_tx = None
        self.log_rx = None

    # ------------------------------------------------------------ 链路
    @property
    def is_open(self) -> bool:
        return self._ser is not None and self._ser.is_open

    def open(self, port: str, baud: int = 115200) -> None:
        self.close()
        try:
            self._ser = serial.Serial(port=port, baudrate=baud, timeout=0.02, write_timeout=2.0)
        except Exception as exc:
            raise LinkError("打开 %s 失败：%s" % (port, exc))
        self._ser.reset_input_buffer()
        self._parser.reset()
        self._seq = 0

    def close(self) -> None:
        if self._ser is not None:
            try:
                self._ser.close()
            except Exception:
                pass
        self._ser = None
        self._parser.reset()

    def _write(self, data: bytes) -> None:
        if not self.is_open:
            raise LinkError("串口未连接")
        try:
            self._ser.write(data)
            self._ser.flush()
        except Exception as exc:
            raise LinkError("发送失败：%s" % exc)
        if self.log_tx:
            self.log_tx(data)

    def _read_raw(self, timeout: float):
        deadline = time.monotonic() + timeout
        while True:
            if not self.is_open:
                raise LinkError("串口已断开")
            try:
                n = self._ser.in_waiting
                data = self._ser.read(n if n else 1)
            except Exception as exc:
                raise LinkError("接收失败：%s" % exc)
            if data:
                if self.log_rx:
                    self.log_rx(data)
                return data
            if time.monotonic() >= deadline:
                return b""

    def _wait_frame(self, cmd: int, seq: int, timeout: float):
        """在超时时间内找到匹配 (cmd, seq) 的应答，返回 payload（含状态字节）"""
        deadline = time.monotonic() + timeout
        while True:
            while self._pending_frames:
                rcmd, rseq, payload = self._pending_frames.pop(0)
                if rcmd == cmd and rseq == seq:
                    return payload        # 其它帧（迟到的应答）直接丢掉
            remain = deadline - time.monotonic()
            if remain <= 0:
                return None
            data = self._read_raw(min(0.05, remain))
            if data:
                self._pending_frames.extend(self._parser.feed(data))

    # ------------------------------------------------------------ 请求
    def request(self, cmd: int, payload: bytes = b"", timeout: float = 3.0, retries: int = 2) -> bytes:
        """发命令并等待应答数据（不含状态字节）。超出重试次数抛 LinkError。"""
        if not self.is_open:
            raise LinkError("串口未连接")
        self._seq = (self._seq + 1) & 0xFF
        seq = self._seq
        frame = build_frame(cmd, seq, payload)
        last = "超时"
        for _ in range(retries + 1):
            self._pending_frames.clear()
            self._write(frame)
            deadline = time.monotonic() + timeout
            while True:
                rsp = self._wait_frame(cmd, seq, max(0.0, deadline - time.monotonic()))
                if rsp is None:
                    break
                status = rsp[0] if rsp else 0xFF
                data = rsp[1:]
                if status != 0:
                    detail = ""
                    if status == 2 and data:
                        from protocol import FRESULT_TEXT
                        detail = FRESULT_TEXT.get(data[0], "FRESULT=%d" % data[0])
                    raise DeviceError(status, detail)
                return data
            last = "命令 %s 无应答" % CMD_NAMES.get(cmd, hex(cmd))
        raise LinkError(last)

    # ------------------------------------------------------------ 设备信息
    def get_info(self) -> dict:
        data = self.request(CMD_SYS_INFO, timeout=3.0)
        if len(data) < 2:
            raise LinkError("设备信息长度异常")
        proto = data[0]
        ver_len = data[1]
        ver = data[2:2 + ver_len].decode("utf-8", "replace")
        i = 2 + ver_len
        info = {"proto": proto, "firmware": ver}
        if len(data) >= i + 1:
            info["param_count"] = data[i]
            i += 1
        if len(data) >= i + 2:
            info["param_bytes"] = struct.unpack("<H", data[i:i + 2])[0]
            i += 2
        if len(data) >= i + 2:
            info["features"] = struct.unpack("<H", data[i:i + 2])[0]
            i += 2
        if len(data) >= i + 4:
            info["bt_baud"] = struct.unpack("<I", data[i:i + 4])[0]
            i += 4
        if len(data) >= i + 4:
            info["total_kb"] = struct.unpack("<I", data[i:i + 4])[0]
            i += 4
        if len(data) >= i + 4:
            info["free_kb"] = struct.unpack("<I", data[i:i + 4])[0]
        return info

    def set_bt_baud(self, baud: int) -> None:
        self.request(CMD_SYS_BT_BAUD, struct.pack("<I", baud), timeout=2.0)

    def reboot(self) -> None:
        try:
            self.request(CMD_SYS_REBOOT, timeout=2.0)
        except LinkError:
            # 设备重启时来不及回复属于正常现象
            pass

    # ------------------------------------------------------------ 参数
    def get_params_raw(self, kind: str = "current") -> bytes:
        data = self.request(CMD_PARAM_GET, bytes([1 if kind == "default" else 0]), timeout=5.0)
        if len(data) < P.PARAM_TOTAL_BYTES:
            raise LinkError("参数数据长度不足（%d/%d）" % (len(data), P.PARAM_TOTAL_BYTES))
        return data[:P.PARAM_TOTAL_BYTES]

    def set_params_raw(self, raw: bytes) -> None:
        if len(raw) != P.PARAM_TOTAL_BYTES:
            raise ValueError("参数长度必须为 %d" % P.PARAM_TOTAL_BYTES)
        self.request(CMD_PARAM_SET, raw, timeout=10.0)

    # ------------------------------------------------------------ 文件系统
    @staticmethod
    def _path_payload(path: str) -> bytes:
        b = path.encode("utf-8")
        if len(b) > 100:
            raise LinkError("路径过长")
        return bytes([len(b)]) + b

    def fs_list(self, path: str):
        entries = []
        start = 0
        while True:
            req = self._path_payload(path) + struct.pack("<HB", start, 24)
            data = self.request(CMD_FS_LIST, req, timeout=5.0)
            if len(data) < 3:
                raise LinkError("目录数据异常")
            count = data[0]
            nxt = struct.unpack("<H", data[1:3])[0]
            pos = 3
            for _ in range(count):
                if pos + 6 > len(data):
                    break
                attr = data[pos]
                size = struct.unpack("<I", data[pos + 1:pos + 5])[0]
                nlen = data[pos + 5]
                name = data[pos + 6:pos + 6 + nlen].decode("utf-8", "replace")
                pos += 6 + nlen
                entries.append({"name": name, "size": size, "attr": attr,
                                "is_dir": bool(attr & FS_ATTR_DIR)})
            if nxt == 0xFFFF or count == 0:
                break
            start = nxt
        return entries

    def fs_read(self, path: str, offset: int, length: int) -> bytes:
        req = self._path_payload(path) + struct.pack("<IH", offset, length)
        data = self.request(CMD_FS_READ, req, timeout=5.0)
        if len(data) < 2:
            raise LinkError("读文件应答异常")
        n = struct.unpack("<H", data[0:2])[0]
        return data[2:2 + n]

    def fs_write(self, path: str, offset: int, chunk: bytes) -> int:
        req = (self._path_payload(path) + struct.pack("<IH", offset, len(chunk)) + chunk)
        data = self.request(CMD_FS_WRITE, req, timeout=10.0)
        if len(data) < 2:
            raise LinkError("写文件应答异常")
        return struct.unpack("<H", data[0:2])[0]

    def fs_delete(self, path: str) -> None:
        self.request(CMD_FS_DELETE, self._path_payload(path), timeout=5.0)

    def fs_mkdir(self, path: str) -> None:
        self.request(CMD_FS_MKDIR, self._path_payload(path), timeout=5.0)

    def fs_rename(self, old: str, new: str) -> None:
        ob = old.encode("utf-8")
        nb = new.encode("utf-8")
        if len(ob) > 100 or len(nb) > 100:
            raise LinkError("路径过长")
        req = bytes([len(ob)]) + ob + bytes([len(nb)]) + nb
        self.request(CMD_FS_RENAME, req, timeout=5.0)

    def fs_stat(self, path: str):
        data = self.request(CMD_FS_STAT, self._path_payload(path), timeout=5.0)
        if len(data) < 5:
            raise LinkError("属性应答异常")
        return data[0], struct.unpack("<I", data[1:5])[0]

    def fs_info(self):
        data = self.request(CMD_FS_INFO, timeout=5.0)
        if len(data) < 8:
            raise LinkError("容量应答异常")
        return struct.unpack("<II", data[0:8])

    def fs_sync(self) -> None:
        self.request(CMD_FS_SYNC, timeout=5.0)

    # ------------------------------------------------------------ 文件传输
    def upload(self, local_path: str, remote_path: str, progress=None, cancel=None) -> None:
        total = os.path.getsize(local_path)
        done = 0
        if total == 0:
            self.fs_write(remote_path, 0, b"")
            if progress:
                progress(0, 0)
        else:
            with open(local_path, "rb") as f:
                while done < total:
                    if cancel is not None and cancel.is_set():
                        raise LinkError("已取消")
                    chunk = f.read(FS_CHUNK)
                    if not chunk:
                        break
                    n = self.fs_write(remote_path, done, chunk)
                    if n != len(chunk):
                        raise LinkError("写入不完整（%d/%d），可能是 Flash 空间不足" % (n, len(chunk)))
                    done += n
                    if progress:
                        progress(done, total)
        self.fs_sync()

    def download(self, remote_path: str, local_path: str, size: int,
                 progress=None, cancel=None) -> None:
        done = 0
        tmp_path = local_path + ".part"
        try:
            with open(tmp_path, "wb") as f:
                while done < size:
                    if cancel is not None and cancel.is_set():
                        raise LinkError("已取消")
                    chunk = self.fs_read(remote_path, done, min(FS_CHUNK, size - done))
                    if not chunk:
                        break
                    f.write(chunk)
                    done += len(chunk)
                    if progress:
                        progress(done, size)
            if done != size:
                raise LinkError("读取不完整（%d/%d）" % (done, size))
            if os.path.exists(local_path):
                os.remove(local_path)
            os.replace(tmp_path, local_path)
        except Exception:
            if os.path.exists(tmp_path):
                try:
                    os.remove(tmp_path)
                except OSError:
                    pass
            raise
