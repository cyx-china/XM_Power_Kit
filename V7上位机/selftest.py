# -*- coding: utf-8 -*-
"""
无硬件自检：验证协议编解码与上位机逻辑（不需要连接设备）。

在 V7上位机 目录下运行：
    python selftest.py
"""
from __future__ import annotations

import os
import struct
import sys
import types

# ---- 未安装 pyserial 时用空壳顶替，保证本文件可以独立运行 ----
try:
    import serial  # noqa: F401
except Exception:
    ser_mod = types.ModuleType("serial")

    class SerialException(Exception):
        pass

    class _DummySerial:
        def __init__(self, *a, **k):
            raise SerialException("no serial port")

    ser_mod.SerialException = SerialException
    ser_mod.Serial = _DummySerial
    tools_mod = types.ModuleType("serial.tools")
    ports_mod = types.ModuleType("serial.tools.list_ports")
    ports_mod.comports = lambda: []
    tools_mod.list_ports = ports_mod
    ser_mod.tools = tools_mod
    sys.modules["serial"] = ser_mod
    sys.modules["serial.tools"] = tools_mod
    sys.modules["serial.tools.list_ports"] = ports_mod

import ota as OTA
import params as P
import protocol as PR
from device import Device


def PR_parse_head(head: bytes) -> dict:
    """把设备包头解析成模拟器内部结构（用上位机同一套代码校验）"""
    info = OTA.parse_head(head)
    return {"version": info["version"], "size": info["image_size"], "crc": info["image_crc"]}


# ============================================================ 模拟设备
class SimDevice:
    def __init__(self):
        self.parser = PR.FrameParser()
        self.files = {"/font/demo.bin": bytearray(bytes(range(256)) * 4)}
        self.dirs = {"/", "/font"}
        self.params = bytearray(P.PARAM_TOTAL_BYTES)
        struct.pack_into("<h", self.params, 0, 123)
        struct.pack_into("<f", self.params, 4, 1.2345)

        # OTA 状态
        self.ota_head = None
        self.ota_image = bytearray()
        self.ota_prev = None
        self.ota_state = 0
        self.ota_fail = 0
        self.ota_req = False
        self.maint = False           # True 表示模拟停在 bootloader 维护模式

    def feed(self, data: bytes) -> bytes:
        out = []
        for cmd, seq, payload in self.parser.feed(data):
            try:
                resp = self._handle(cmd, payload)
            except Exception as exc:                      # noqa: BLE001
                resp = bytes([PR.ST_E_FS, 0]) + str(exc).encode()
            out.append(PR.build_frame(cmd, seq, resp))
        return b"".join(out)

    def _path(self, payload, i=1, n=None):
        if n is None:
            n = payload[0]
        return payload[i:i + n].decode("utf-8")

    def _handle(self, cmd, p):
        if cmd == PR.CMD_SYS_INFO:
            if self.maint:
                ver = b"XM Power Kit Bootloader"
                features = 0x001D          # USB|FS|OTA|维护模式
            else:
                ver = b"STM32F407VE.TEST.Rel.9999"
                features = 0x0007
            body = bytes([PR.PROTO_VER, len(ver)]) + ver
            body += bytes([0 if self.maint else P.PARAM_TOTAL_COUNT])
            body += struct.pack("<HHII", 0 if self.maint else P.PARAM_TOTAL_BYTES,
                                features, 0 if self.maint else 115200, 0)
            body += struct.pack("<II", 32768, 20000)
            return b"\x00" + body

        if cmd == PR.CMD_PARAM_GET:
            return b"\x00" + bytes(self.params)

        if cmd == PR.CMD_PARAM_SET:
            assert len(p) == P.PARAM_TOTAL_BYTES, len(p)
            self.params = bytearray(p)
            return b"\x00"

        if cmd == PR.CMD_FS_INFO:
            return b"\x00" + struct.pack("<II", 32768, 20000)

        if cmd == PR.CMD_FS_LIST:
            plen = p[0]
            path = self._path(p, 1, plen)
            start, maxn = struct.unpack("<HB", p[1 + plen:4 + plen])
            names = []
            prefix = "/" if path == "/" else path + "/"
            for d in sorted(self.dirs):
                if d != "/" and d.rsplit("/", 1)[0] == (path.rstrip("/") or "/"):
                    names.append((d.rsplit("/", 1)[-1], 0x10, 0))
            for f, content in sorted(self.files.items()):
                if f.rsplit("/", 1)[0] == (path.rstrip("/") or "/"):
                    names.append((f.rsplit("/", 1)[-1], 0x20, len(content)))
            names = names[start:start + maxn]
            body = bytearray([len(names)])
            nxt = 0xFFFF if start + len(names) >= len(names) + start else start + len(names)
            body += struct.pack("<H", nxt if start + len(names) < len(names) + start else 0xFFFF)
            for name, attr, size in names:
                nb = name.encode()
                body += bytes([attr]) + struct.pack("<I", size) + bytes([len(nb)]) + nb
            return b"\x00" + bytes(body)

        if cmd == PR.CMD_FS_READ:
            plen = p[0]
            path = self._path(p, 1, plen)
            off, length = struct.unpack("<IH", p[1 + plen:7 + plen])
            data = self.files.get(path)
            if data is None:
                return bytes([PR.ST_E_NOTFOUND, 4])
            chunk = bytes(data[off:off + length])
            return b"\x00" + struct.pack("<H", len(chunk)) + chunk

        if cmd == PR.CMD_FS_WRITE:
            plen = p[0]
            path = self._path(p, 1, plen)
            off, length = struct.unpack("<IH", p[1 + plen:7 + plen])
            chunk = p[7 + plen:7 + plen + length]
            if off == 0:
                self.files[path] = bytearray()
            buf = self.files.setdefault(path, bytearray())
            if len(buf) < off:
                buf.extend(b"\x00" * (off - len(buf)))
            buf[off:off + len(chunk)] = chunk
            return b"\x00" + struct.pack("<H", len(chunk))

        if cmd == PR.CMD_FS_DELETE:
            path = self._path(p)
            if path in self.files:
                del self.files[path]
                return b"\x00"
            return bytes([PR.ST_E_NOTFOUND, 4])

        if cmd == PR.CMD_FS_MKDIR:
            self.dirs.add(self._path(p))
            return b"\x00"

        if cmd == PR.CMD_FS_RENAME:
            l1 = p[0]
            old = p[1:1 + l1].decode()
            l2 = p[1 + l1]
            new = p[2 + l1:2 + l1 + l2].decode()
            if old in self.files:
                self.files[new] = self.files.pop(old)
            return b"\x00"

        if cmd == PR.CMD_FS_STAT:
            path = self._path(p)
            if path in self.files:
                return b"\x00" + bytes([0x20]) + struct.pack("<I", len(self.files[path]))
            return bytes([PR.ST_E_NOTFOUND, 4])

        if cmd == PR.CMD_OTA_APPLY:
            # 维护模式：校验 /ota/FW.BIN 后写升级请求（真实设备随后重启搬运）
            pkg = self.files.get("/ota/FW.BIN")
            if not pkg:
                return bytes([PR.ST_E_NOTFOUND, 4])
            head = PR_parse_head(bytes(pkg[:PR.OTA_HEAD_SIZE]))
            self.ota_head = head
            self.ota_image = bytearray(pkg[PR.OTA_HEAD_SIZE:])
            self.ota_req = True
            return b"\x00"

        if cmd in (PR.CMD_FS_SYNC, PR.CMD_SYS_REBOOT, PR.CMD_SYS_BT_BAUD):
            return b"\x00"

        # ---------------- OTA ----------------
        if cmd == PR.CMD_OTA_INFO:
            good = self.ota_prev or {}
            head = self.ota_head or {}
            flags = (0x01 if self.ota_prev else 0) | (0x02 if self.ota_head else 0) | \
                    (0x04 if self.ota_req else 0)
            body = bytes([flags, 1 if self.ota_head else 0])
            body += struct.pack("<I", 303020)
            body += (good.get("version", "") or "").encode().ljust(32, b"\0")[:32]
            body += struct.pack("<I", good.get("size", 0))
            body += (head.get("version", "") or "").encode().ljust(32, b"\0")[:32]
            body += struct.pack("<I", head.get("size", 0))
            body += struct.pack("<I", head.get("crc", 0))
            body += bytes([self.ota_fail, self.ota_state])
            return b"\x00" + body

        if cmd == PR.CMD_OTA_BEGIN:
            assert len(p) == PR.OTA_HEAD_SIZE, len(p)
            info = PR_parse_head(p)
            self.ota_head = info
            self.ota_image = bytearray()
            return b"\x00" + struct.pack("<I", 0)

        if cmd == PR.CMD_OTA_DATA:
            off = struct.unpack("<I", p[0:4])[0]
            chunk = p[4:]
            expect = PR.OTA_HEAD_SIZE + len(self.ota_image)
            if off != expect:
                return bytes([PR.ST_E_PARAM]) + struct.pack("<I", expect)
            self.ota_image.extend(chunk)
            return b"\x00" + struct.pack("<I", len(self.ota_image))

        if cmd == PR.CMD_OTA_COMMIT:
            if self.ota_head is None:
                return bytes([PR.ST_E_PARAM])
            if len(self.ota_image) != self.ota_head["size"]:
                return bytes([PR.ST_E_PARAM])
            if PR.crc32(bytes(self.ota_image)) != self.ota_head["crc"]:
                return bytes([PR.ST_E_CRC])
            self.ota_prev = self.ota_head
            self.ota_req = True
            self.ota_state = 1
            return b"\x00"

        if cmd == PR.CMD_OTA_ABORT:
            self.ota_head = None
            self.ota_image = bytearray()
            return b"\x00"

        if cmd == PR.CMD_OTA_ROLLBACK:
            if not self.ota_prev:
                return bytes([PR.ST_E_NOTFOUND])
            self.ota_req = True
            return b"\x00"

        return bytes([PR.ST_E_UNSUPPORTED])


class FakeDevice(Device):
    def __init__(self):
        super().__init__()
        self.sim = SimDevice()
        self._rx = b""

    @property
    def is_open(self):
        return True

    def _write(self, data):
        self._rx += self.sim.feed(data)

    def _read_raw(self, timeout):
        if self._rx:
            data, self._rx = self._rx, b""
            return data
        return b""


# ============================================================ 测试
def main() -> int:
    # 1) CRC16 标准校验向量
    assert PR.crc16(b"123456789") == 0x29B1, hex(PR.crc16(b"123456789"))

    # 2) 帧往返
    frame = PR.build_frame(0x10, 7, b"\x01\x02\x03")
    assert frame[:2] == bytes((0xA5, 0x5A))
    assert PR.FrameParser().feed(frame) == [(0x10, 7, b"\x01\x02\x03")]

    # 3) 分片 + 噪声 + 坏帧重同步
    bad = bytes((0xA5, 0x5A, 0x03, 0x00, 0x10, 0x08, 1, 2, 3, 0x00, 0x00))
    data = b"\x00\x11" + frame + bad + frame
    parser = PR.FrameParser()
    got = []
    for i in range(0, len(data), 5):
        got.extend(parser.feed(data[i:i + 5]))
    assert len(got) == 2, got

    # 4) 参数编解码
    vals = {0: -5, 4: 42, 1: 3.5, 49: 80}
    raw = P.encode_params(vals)
    assert len(raw) == P.PARAM_TOTAL_BYTES
    back = P.decode_params(raw)
    assert back[0] == -5 and back[4] == 42 and abs(back[1] - 3.5) < 1e-6 and back[49] == 80

    # 5) 设备交互
    dev = FakeDevice()
    info = dev.get_info()
    assert info["firmware"].startswith("STM32F407VE"), info
    assert info["param_count"] == P.PARAM_TOTAL_COUNT

    cur = P.decode_params(dev.get_params_raw("current"))
    assert cur[0] == 123 and abs(cur[1] - 1.2345) < 1e-4, cur[0:2]

    entries = dev.fs_list("/font")
    names = [e["name"] for e in entries]
    assert names == ["demo.bin"], names
    assert entries[0]["size"] == 1024

    total, free = dev.fs_info()
    assert (total, free) == (32768, 20000)

    # 上传 -> 下载 一致性
    src = os.path.join(os.path.dirname(os.path.abspath(__file__)), "_selftest_src.bin")
    dst = src + ".down"
    payload = bytes((i * 7) & 0xFF for i in range(1500))
    with open(src, "wb") as f:
        f.write(payload)
    dev.upload(src, "/font/up.bin")
    assert dev.fs_stat("/font/up.bin")[1] == len(payload)
    dev.download("/font/up.bin", dst, len(payload))
    with open(dst, "rb") as f:
        assert f.read() == payload
    os.remove(src)
    os.remove(dst)

    dev.fs_rename("/font/up.bin", "/font/up2.bin")
    assert "/font/up2.bin" in dev.sim.files
    dev.fs_delete("/font/up2.bin")
    assert "/font/up2.bin" not in dev.sim.files

    # 错误状态码 -> DeviceError
    try:
        dev.fs_delete("/font/nothing.bin")
        raise AssertionError("应当抛出 DeviceError")
    except PR.DeviceError as exc:
        assert exc.status == PR.ST_E_NOTFOUND

    # 6) OTA 升级包与升级流程
    image = bytes(200) + b"STM32F407VE.261108.Rel.0007\x00" + bytes(range(256)) * 8
    bin_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "_selftest_fw.bin")
    with open(bin_path, "wb") as f:
        f.write(image)
    try:
        pkg = OTA.make_package(bin_path)
        assert pkg["version"] == "STM32F407VE.261108.Rel.0007", pkg["version"]
        assert pkg["size"] == len(image)
        assert pkg["crc"] == PR.crc32(image)
        head_info = OTA.parse_head(pkg["head"])
        assert head_info["image_size"] == len(image)
        assert head_info["load_addr"] == PR.OTA_APP_ADDR
        assert head_info["board"] == "XM_Power_Kit_V7"

        # 偏移不符 -> OtaResync
        dev.ota_begin(pkg["head"])
        try:
            dev.ota_data(PR.OTA_HEAD_SIZE + 5, b"abc")
            raise AssertionError("应当抛 OtaResync")
        except PR.OtaResync as exc:
            assert exc.offset == PR.OTA_HEAD_SIZE
        dev.ota_abort()

        # 完整上传（含多块）
        dev.ota_upload(pkg["head"], bin_path)
        assert dev.sim.ota_req is True
        assert bytes(dev.sim.ota_image) == image

        info = dev.ota_info()
        assert info["has_prev"] and info["has_fw"]
        assert info["prev_version"] == pkg["version"]
        assert info["prev_size"] == len(image)
        assert info["state"] == 1
        dev.ota_rollback()
        assert dev.sim.ota_req is True

        # 版本识别失败时要求手动填写
        plain = b"\x00" * 64
        with open(bin_path, "wb") as f:
            f.write(plain)
        try:
            OTA.make_package(bin_path)
            raise AssertionError("没有版本串应当报错")
        except ValueError:
            pass
        with open(bin_path, "wb") as f:
            f.write(b"\x00" * 8 + b"STM32F407VE.261108.Rel.0007\x00" + bytes(200) * 2000)
        try:
            OTA.make_package(bin_path)       # 401 KB 还行，下面试超限
            big = b"\x00" * 8 + b"STM32F407VE.261108.Rel.0007\x00" + bytes(PR.OTA_APP_MAX)
            with open(bin_path, "wb") as f:
                f.write(big)
            OTA.make_package(bin_path)
            raise AssertionError("超过分区上限应当报错")
        except ValueError:
            pass
        # 7) 维护模式（bootloader 常驻）一键升级
        sim_maint = FakeDevice()
        sim_maint.sim.maint = True
        minfo = sim_maint.get_info()
        assert minfo["features"] & PR.FEATURE_MAINT, minfo
        assert sim_maint.maintenance is True
        assert sim_maint.get_info()["param_count"] == 0
        image2 = bytes(64) + b"STM32F407VE.261108.Rel.0007\x00" + bytes(range(128)) * 6
        with open(bin_path, "wb") as f:
            f.write(image2)
        pkg2 = OTA.make_package(bin_path)
        head2 = OTA.build_head(pkg2["version"], pkg2["size"], pkg2["crc"], pkg2["build_time"])
        sim_maint.ota_maintenance_upgrade(head2, bin_path)
        written = bytes(sim_maint.sim.files["/ota/FW.BIN"])
        # 上传后会按“当前文件”重建包头并回读校验，所以按同一套规则再算一遍比对
        fresh = OTA.make_package(bin_path)
        assert written == fresh["head"] + image2, (len(written), len(fresh["head"]) + len(image2))
        assert sim_maint.sim.ota_req is True
        assert bytes(sim_maint.sim.ota_image) == image2
    finally:
        if os.path.exists(bin_path):
            os.remove(bin_path)

    print("全部自检通过：协议编解码 / 参数 / 目录 / 上传 / 下载 / 重命名 / 删除 / 错误码 / "
          "OTA 升级包 / 维护模式一键升级")
    return 0


if __name__ == "__main__":
    sys.exit(main())
