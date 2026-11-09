# -*- coding: utf-8 -*-
"""后台工作线程：串口收发与文件传输都在这里执行，避免卡住界面。"""
from __future__ import annotations

import os
import queue
import threading
import traceback

from PyQt5.QtCore import QThread, pyqtSignal

import filetree
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
        self._done_msg = ""          # 任务结束时附带的说明（批量操作的汇总）
        self._done_ok = True         # 批量操作是否有失败项
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
            self._done_msg = ""
            self._done_ok = True
            try:
                handler = getattr(self, "_job_" + name)
                handler(*args)
                self.sig_done.emit(tag, self._done_ok, self._done_msg)
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

    # ------------------------------------------------------------ 连接与参数
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

    # ------------------------------------------------------------ 文件浏览
    def _job_list(self, path: str) -> None:
        entries = self._dev.fs_list(path)
        entries.sort(key=lambda e: (not e["is_dir"], e["name"].lower()))
        self.sig_dir.emit(path, entries)
        try:
            total, free = self._dev.fs_info()
            self.sig_capacity.emit(total, free)
        except (DeviceError, LinkError):
            pass

    def _job_mkdir(self, path: str) -> None:
        self._dev.fs_mkdir(path)
        self.sig_status.emit("已创建目录：%s" % path)
        self.sig_done.emit("refresh_dir", True, "")

    def _job_rename(self, old: str, new: str) -> None:
        self._dev.fs_rename(old, new)
        self.sig_status.emit("已重命名：%s -> %s" % (old, new))
        self.sig_done.emit("refresh_dir", True, "")

    # ------------------------------------------------------------ 批量文件操作
    def _job_upload_tree(self, local_paths, remote_dir: str) -> None:
        """拖拽/多选上传：文件直接传，文件夹递归建目录后传"""
        dirs, plan, total, skipped = filetree.build_upload_plan(local_paths, remote_dir)
        self.sig_status.emit("正在上传 %d 个文件，共 %s …"
                             % (len(plan), filetree.human_size(total)))
        for d in dirs:
            if self._cancel.is_set():
                raise LinkError("已取消")
            self._dev.ensure_dir(d)

        failed = []
        done = 0
        for i, (local, remote, size) in enumerate(plan, 1):
            if self._cancel.is_set():
                raise LinkError("已取消")
            self.sig_status.emit("上传 %d/%d：%s" % (i, len(plan), os.path.basename(local)))
            base = done
            try:
                self._dev.upload(local, remote,
                                 lambda d, t, base=base: self._progress(base + d, total),
                                 self._cancel)
            except DeviceError as exc:            # 单个文件失败不中断整批
                failed.append("%s：%s" % (remote, exc))
            done += size
            self._progress(done, total)

        self.sig_done.emit("refresh_dir", True, "")
        msg = "已上传 %d 个文件，共 %s" % (len(plan) - len(failed), filetree.human_size(total))
        if skipped:
            msg += "；跳过 %d 个（路径超过 %d 字节）" % (len(skipped), filetree.MAX_REMOTE_PATH)
        if failed:
            msg += "；%d 个失败：\n%s" % (len(failed), "\n".join(failed[:8]))
            self._done_ok = False
        self._done_msg = msg

    def _job_download_tree(self, remotes, local_dir: str) -> None:
        """批量下载：文件直接下，文件夹按远端结构镜像到本地目录"""
        dirs, plan = filetree.build_download_plan(self._dev, remotes)
        total = sum(size for _rp, size, _rel in plan)
        for rel in dirs:
            os.makedirs(os.path.join(local_dir, *rel.split("/")), exist_ok=True)

        self.sig_status.emit("正在下载 %d 个文件，共 %s …"
                             % (len(plan), filetree.human_size(total)))
        failed = []
        done = 0
        for i, (remote, size, rel) in enumerate(plan, 1):
            if self._cancel.is_set():
                raise LinkError("已取消")
            self.sig_status.emit("下载 %d/%d：%s" % (i, len(plan), rel))
            dst = os.path.join(local_dir, *rel.split("/"))
            base = done
            try:
                self._dev.download(remote, dst, size,
                                   lambda d, t, base=base: self._progress(base + d, total),
                                   self._cancel)
            except DeviceError as exc:
                failed.append("%s：%s" % (rel, exc))
            done += size
            self._progress(done, total)

        msg = "已下载 %d 个文件，共 %s" % (len(plan) - len(failed), filetree.human_size(total))
        if failed:
            msg += "；%d 个失败：\n%s" % (len(failed), "\n".join(failed[:8]))
            self._done_ok = False
        self._done_msg = msg

    def _job_delete_tree(self, remotes) -> None:
        """批量删除：文件夹递归删（先文件后目录，设备端不能删非空目录）"""
        dirs, files = filetree.build_delete_plan(self._dev, remotes)
        total = len(files) + len(dirs)
        file_set = set(files)
        ok_files = ok_dirs = 0
        failed = []
        done = 0
        for rp in files + dirs:
            if self._cancel.is_set():
                raise LinkError("已取消")
            self.sig_status.emit("删除 %s …" % rp)
            try:
                self._dev.fs_delete(rp)
                if rp in file_set:
                    ok_files += 1
                else:
                    ok_dirs += 1
            except DeviceError as exc:
                failed.append("%s：%s" % (rp, exc))
            done += 1
            self._progress(done, total)

        self.sig_done.emit("refresh_dir", True, "")
        msg = "已删除 %d 个文件、%d 个文件夹" % (ok_files, ok_dirs)
        if failed:
            msg += "；%d 项失败：\n%s" % (len(failed), "\n".join(failed[:8]))
            self._done_ok = False
        self._done_msg = msg

    # ------------------------------------------------------------ 其他
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
