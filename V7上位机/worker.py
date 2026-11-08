# -*- coding: utf-8 -*-
"""后台工作线程：串口收发与文件传输都在这里执行，避免卡住界面。"""
from __future__ import annotations

import queue
import threading
import traceback

from PyQt5.QtCore import QThread, pyqtSignal

import params as P
from device import Device
from protocol import DeviceError, LinkError


class Worker(QThread):
    sig_status = pyqtSignal(str)              # 状态栏文字
    sig_connected = pyqtSignal(bool, str)     # 是否连接, 说明
    sig_info = pyqtSignal(dict)               # 设备信息
    sig_params = pyqtSignal(str, dict)        # kind, {id: 值}
    sig_dir = pyqtSignal(str, list)           # path, entries
    sig_capacity = pyqtSignal(int, int)       # total_kb, free_kb
    sig_progress = pyqtSignal(int, int)       # done, total
    sig_ota_info = pyqtSignal(dict)           # 升级状态
    sig_done = pyqtSignal(str, bool, str)     # tag, ok, message
    sig_log = pyqtSignal(str)

    def __init__(self) -> None:
        super().__init__()
        self._jobs = queue.Queue()
        self._dev = Device()
        self._cancel = threading.Event()
        self._alive = True
        self._dev.log_tx = self._log_tx
        self._dev.log_rx = self._log_rx

    # ------------------------------------------------------------ 接口
    def submit(self, tag: str, name: str, *args) -> None:
        self._jobs.put((tag, name, args))

    def cancel_current(self) -> None:
        self._cancel.set()

    def shutdown(self) -> None:
        self._alive = False
        self._cancel.set()
        self._dev.close()
        self.wait(1500)

    @property
    def device(self) -> Device:
        return self._dev

    @property
    def cancel_flag(self):
        return self._cancel

    # ------------------------------------------------------------ 线程体
    def run(self) -> None:
        while self._alive:
            try:
                tag, name, args = self._jobs.get(timeout=0.2)
            except queue.Empty:
                continue
            self._cancel.clear()
            try:
                handler = getattr(self, "_job_" + name)
                handler(*args)
                self.sig_done.emit(tag, True, "")
            except DeviceError as exc:
                self.sig_done.emit(tag, False, str(exc))
            except LinkError as exc:
                self.sig_done.emit(tag, False, str(exc))
            except Exception as exc:                      # noqa: BLE001
                traceback.print_exc()
                self.sig_done.emit(tag, False, "%s: %s" % (type(exc).__name__, exc))

    def _log_tx(self, data: bytes) -> None:
        self.sig_log.emit("TX " + data.hex(" "))

    def _log_rx(self, data: bytes) -> None:
        self.sig_log.emit("RX " + data.hex(" "))

    def _progress(self, done: int, total: int) -> None:
        if self._cancel.is_set():
            raise LinkError("已取消")
        self.sig_progress.emit(done, total)

    # ------------------------------------------------------------ 任务
    def _job_connect(self, port: str, baud: int) -> None:
        self.sig_status.emit("正在连接 %s ..." % port)
        self._dev.open(port, baud)
        try:
            info = self._dev.get_info()
        except Exception:
            self._dev.close()
            raise
        self.sig_info.emit(info)          # 先给信息：主窗口据此判断维护模式
        self.sig_connected.emit(True, port)
        try:
            total, free = self._dev.fs_info()
            self.sig_capacity.emit(total, free)
        except (DeviceError, LinkError):
            pass
        self.sig_status.emit("已连接 %s，固件 %s" % (port, info.get("firmware", "?")))

    def _job_disconnect(self) -> None:
        self._dev.close()
        self.sig_connected.emit(False, "")
        self.sig_status.emit("已断开")

    def _job_info(self) -> None:
        info = self._dev.get_info()
        self.sig_info.emit(info)
        total, free = self._dev.fs_info()
        self.sig_capacity.emit(total, free)

    def _job_read_params(self, kind: str) -> None:
        raw = self._dev.get_params_raw(kind)
        self.sig_params.emit(kind, P.decode_params(raw))
        self.sig_status.emit("已读取%s参数" % ("默认" if kind == "default" else "当前"))

    def _job_write_params(self, raw: bytes) -> None:
        self._dev.set_params_raw(raw)
        self.sig_status.emit("参数写入完成（部分设置需重启设备生效）")

    def _job_list(self, path: str) -> None:
        entries = self._dev.fs_list(path)
        entries.sort(key=lambda e: (not e["is_dir"], e["name"].lower()))
        self.sig_dir.emit(path, entries)
        try:
            total, free = self._dev.fs_info()
            self.sig_capacity.emit(total, free)
        except (DeviceError, LinkError):
            pass

    def _job_upload(self, local_path: str, remote_path: str) -> None:
        self.sig_status.emit("正在上传 %s ..." % remote_path)
        self._dev.upload(local_path, remote_path, self._progress, self._cancel)
        self.sig_status.emit("上传完成：%s" % remote_path)
        self.sig_done.emit("refresh_dir", True, "")

    def _job_download(self, remote_path: str, local_path: str, size: int) -> None:
        self.sig_status.emit("正在下载 %s ..." % remote_path)
        self._dev.download(remote_path, local_path, size, self._progress, self._cancel)
        self.sig_status.emit("下载完成：%s" % local_path)

    def _job_delete(self, path: str) -> None:
        self._dev.fs_delete(path)
        self.sig_status.emit("已删除：%s" % path)
        self.sig_done.emit("refresh_dir", True, "")

    def _job_mkdir(self, path: str) -> None:
        self._dev.fs_mkdir(path)
        self.sig_status.emit("已创建目录：%s" % path)
        self.sig_done.emit("refresh_dir", True, "")

    def _job_rename(self, old: str, new: str) -> None:
        self._dev.fs_rename(old, new)
        self.sig_status.emit("已重命名：%s -> %s" % (old, new))
        self.sig_done.emit("refresh_dir", True, "")

    def _job_bt_baud(self, baud: int) -> None:
        self._dev.set_bt_baud(baud)
        self.sig_status.emit("蓝牙波特率已设为 %d（重新连接蓝牙后生效）" % baud)

    def _job_reboot(self) -> None:
        self._dev.reboot()
        self.sig_status.emit("设备已重启")

    # ------------------------------------------------------------ OTA
    def _job_ota_info(self) -> None:
        self.sig_ota_info.emit(self._dev.ota_info())

    def _job_ota_upload(self, head: bytes, bin_path: str) -> None:
        if self._dev.maintenance:
            # bootloader 维护模式：上传打包好的 FW.BIN，再让设备立刻搬运安装
            self.sig_status.emit("正在上传固件（维护模式）…")
            self._dev.ota_maintenance_upgrade(head, bin_path, self._progress, self._cancel)
            self.sig_status.emit("固件已提交，设备正在写入内部 Flash…")
        else:
            self.sig_status.emit("正在上传固件…")
            self._dev.ota_upload(head, bin_path, self._progress, self._cancel)
            self.sig_status.emit("固件已写入，设备正在重启安装…")
        self._dev.close()
        self.sig_connected.emit(False, "")

    def _job_ota_rollback(self) -> None:
        self.sig_status.emit("正在请求回滚…")
        self._dev.ota_rollback()
        self.sig_status.emit("设备正在回滚并重启…")
        self._dev.close()
        self.sig_connected.emit(False, "")
