# -*- coding: utf-8 -*-
"""固件升级页：选择 .bin，校验后写入设备并安装。

两种模式（连接后按设备特性位自动切换）：
    正常运行（主程序）：OTA_BEGIN 写包头 → OTA_DATA 分块传镜像 → OTA_COMMIT
        → 设备生成回滚快照、写升级请求、自动重启 → bootloader 搬运安装
    维护模式（bootloader 常驻）：上传打包好的 FW.BIN → OTA_APPLY
        → bootloader 立刻搬运安装，适合新板子/固件无法启动时救砖
"""
from __future__ import annotations

import os

from PyQt5.QtCore import Qt, pyqtSignal
from PyQt5.QtGui import QFont
from PyQt5.QtWidgets import (
    QFileDialog, QGridLayout, QGroupBox, QHBoxLayout, QLabel, QLineEdit,
    QMessageBox, QPlainTextEdit, QProgressBar, QPushButton, QVBoxLayout, QWidget,
)

import ota

_STATE_TEXT = {0: "空闲（可正常启动）", 1: "已升级，等待应用确认启动成功"}


def fmt_size(n: int) -> str:
    if n < 1024:
        return "%d B" % n
    if n < 1024 * 1024:
        return "%.1f KB" % (n / 1024.0)
    return "%.2f MB" % (n / 1024.0 / 1024.0)


class OtaView(QWidget):
    request_info = pyqtSignal()
    request_upgrade = pyqtSignal(bytes, str)      # 80 字节包头, 本地 bin 路径
    request_rollback = pyqtSignal()
    request_cancel = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self._pkg = None
        self._maint = False
        self._build()

    # ------------------------------------------------------------ 界面
    def _build(self) -> None:
        root = QVBoxLayout(self)
        root.setContentsMargins(8, 8, 8, 8)
        root.setSpacing(8)

        # ---- 固件包 ----
        pkg_box = QGroupBox("固件包")
        pg = QGridLayout(pkg_box)
        pg.setColumnStretch(1, 1)

        self.file_edit = QLineEdit()
        self.file_edit.setReadOnly(True)
        self.file_edit.setPlaceholderText("选择编译输出的 XM_Power_Kit.bin（或已打包的 .xmkota）")
        self.btn_browse = QPushButton("选择文件")
        pg.addWidget(QLabel("文件"), 0, 0)
        pg.addWidget(self.file_edit, 0, 1)
        pg.addWidget(self.btn_browse, 0, 2)

        self.version_edit = QLineEdit()
        self.version_edit.setPlaceholderText("从镜像里自动识别，识别不到请手动填写")
        pg.addWidget(QLabel("版本"), 1, 0)
        pg.addWidget(self.version_edit, 1, 1, 1, 2)

        mono = QFont("Consolas")
        mono.setStyleHint(QFont.Monospace)
        self.size_label = QLabel("—")
        self.crc_label = QLabel("—")
        self.crc_label.setFont(mono)
        pg.addWidget(QLabel("大小"), 2, 0)
        pg.addWidget(self.size_label, 2, 1)
        pg.addWidget(QLabel("CRC32"), 3, 0)
        pg.addWidget(self.crc_label, 3, 1, 1, 2)
        root.addWidget(pkg_box)

        # ---- 设备状态 ----
        dev_box = QGroupBox("设备状态")
        self.dev_box = dev_box
        dg = QGridLayout(dev_box)
        dg.setColumnStretch(1, 1)
        self.labels = {}
        for r, (key, title) in enumerate([
            ("app", "应用分区占用"),
            ("prev", "可回滚版本（PREV.BIN）"),
            ("fw", "待安装包（FW.BIN）"),
            ("state", "升级状态"),
            ("fail", "连续失败次数"),
        ]):
            dg.addWidget(QLabel(title), r, 0)
            value = QLabel("—")
            value.setTextInteractionFlags(Qt.TextSelectableByMouse)
            dg.addWidget(value, r, 1)
            self.labels[key] = value
        root.addWidget(dev_box)

        # ---- 操作 ----
        bar = QHBoxLayout()
        self.btn_upgrade = QPushButton("开始升级")
        self.btn_upgrade.setObjectName("PrimaryButton")
        self.btn_rollback = QPushButton("回滚到上一版本")
        self.btn_refresh = QPushButton("刷新状态")
        self.btn_cancel = QPushButton("取消")
        self.btn_cancel.setVisible(False)
        for b in (self.btn_upgrade, self.btn_rollback, self.btn_refresh):
            bar.addWidget(b)
        bar.addStretch(1)
        bar.addWidget(self.btn_cancel)
        root.addLayout(bar)

        self.progress = QProgressBar()
        self.progress.setRange(0, 1000)
        self.progress.setValue(0)
        self.progress.setVisible(False)
        root.addWidget(self.progress)

        self.log = QPlainTextEdit()
        self.log.setReadOnly(True)
        self.log.setFont(mono)
        self.log.setPlaceholderText("升级过程日志")
        root.addWidget(self.log, 1)

        self.tip = QLabel("升级期间请勿断电。设备会自动重启并进入 bootloader，屏幕会显示安装进度，"
                          "完成后自动跳转到新固件。")
        self.tip.setStyleSheet("color:#a6adc8;")
        root.addWidget(self.tip)

        self.btn_browse.clicked.connect(self._browse)
        self.btn_upgrade.clicked.connect(self._upgrade)
        self.btn_rollback.clicked.connect(self._rollback)
        self.btn_refresh.clicked.connect(self.request_info.emit)
        self.btn_cancel.clicked.connect(self.request_cancel.emit)

    # ------------------------------------------------------------ 交互
    def _browse(self) -> None:
        path, _ = QFileDialog.getOpenFileName(
            self, "选择固件", "", "固件 (*.bin *.xmkota);;所有文件 (*)")
        if not path:
            return
        try:
            pkg = ota.make_package(path)
        except Exception as exc:                       # noqa: BLE001
            QMessageBox.warning(self, "文件有误", str(exc))
            return
        self._pkg = pkg
        self.btn_upgrade.setEnabled(True)      # 选好文件后按钮要能点（以前要断线重连才行）
        self.file_edit.setText(path)
        self.version_edit.setText(pkg["version"])
        self.size_label.setText("%s（%d 字节）" % (fmt_size(pkg["size"]), pkg["size"]))
        self.crc_label.setText("0x%08X" % pkg["crc"])
        self.append_log("已载入 %s：版本 %s，%d 字节，CRC 0x%08X"
                        % (os.path.basename(path), pkg["version"], pkg["size"], pkg["crc"]))
        if not ota.sniff_version(pkg["image"]):
            self.append_log("提示：镜像里没找到版本串，请确认「版本」填写正确（用于启动确认比对）")

    def _upgrade(self) -> None:
        path = self.file_edit.text().strip()
        if not path:
            QMessageBox.information(self, "提示", "请先选择固件文件")
            return
        version = self.version_edit.text().strip()
        # 按“现在磁盘上的文件”重新打包：固件重新编译过也不会出现包头对不上
        try:
            pkg = ota.make_package(path, version)
        except Exception as exc:                       # noqa: BLE001
            QMessageBox.warning(self, "文件有误", str(exc))
            return
        self._pkg = pkg
        self.version_edit.setText(pkg["version"])
        self.size_label.setText("%s（%d 字节）" % (fmt_size(pkg["size"]), pkg["size"]))
        self.crc_label.setText("0x%08X" % pkg["crc"])
        head = pkg["head"]
        if self._maint:
            hint = ("设备当前处于维护模式（bootloader）。\n\n"
                    "把固件 %s（%s）上传到设备并立刻写入内部 Flash？\n\n"
                    "写入期间请勿断电。" % (version, fmt_size(pkg["size"])))
        else:
            hint = ("把固件 %s（%s）写入设备并重启安装？\n\n升级期间请勿断电。"
                    % (version, fmt_size(pkg["size"])))
        if QMessageBox.question(self, "确认升级", hint) != QMessageBox.Yes:
            return
        self.append_log("开始升级：%s，%s" % (version, fmt_size(pkg["size"])))
        self.request_upgrade.emit(head, self.file_edit.text())

    def _rollback(self) -> None:
        if QMessageBox.question(
                self, "确认回滚",
                "用外部 Flash 里的 PREV.BIN（升级前的版本）覆盖内部 Flash 并重启？\n\n"
                "仅在当前固件无法正常使用时才需要。") != QMessageBox.Yes:
            return
        self.append_log("请求回滚到上一版本…")
        self.request_rollback.emit()

    # ------------------------------------------------------------ 模式
    def set_mode(self, maint: bool) -> None:
        """维护模式（boot 常驻）下只能“写入并安装”，没有回滚与升级状态"""
        self._maint = maint
        self.btn_upgrade.setText("写入并安装" if maint else "开始升级")
        self.btn_rollback.setVisible(not maint)
        self.dev_box.setTitle("设备状态（维护模式不提供升级状态）" if maint else "设备状态")
        if maint:
            for v in self.labels.values():
                v.setText("—")
        self.tip.setText(
            "设备停在 bootloader 维护模式：固件会直接写入内部 Flash 并重启，"
            "适合新板子首次安装或固件无法启动时恢复。升级期间请勿断电。"
            if maint else
            "升级期间请勿断电。设备会自动重启并进入 bootloader，屏幕会显示安装进度，"
            "完成后自动跳转到新固件。")

    # ------------------------------------------------------------ 状态更新
    def set_info(self, info: dict) -> None:
        self.labels["app"].setText(fmt_size(info.get("app_size", 0)))
        self.labels["prev"].setText(
            "%s（%s）" % (info["prev_version"], fmt_size(info["prev_size"]))
            if info.get("has_prev") and info.get("prev_version") else
            ("存在（版本未知）" if info.get("has_prev") else "无"))
        self.labels["fw"].setText(
            "%s（%s）" % (info["fw_version"], fmt_size(info["fw_size"]))
            if info.get("has_fw") and info.get("fw_version") else
            ("存在（版本未知）" if info.get("has_fw") else "无"))
        state = _STATE_TEXT.get(info.get("state", 0), "未知(%d)" % info.get("state", 0))
        if info.get("uploading"):
            state += " · 正在上传"
        self.labels["state"].setText(state)
        fail = info.get("fail_count", 0)
        self.labels["fail"].setText(str(fail) + ("（连续 3 次将自动回滚）" if fail else ""))
        self.btn_rollback.setEnabled(bool(info.get("has_prev")) and not self._maint)

    def set_progress(self, done: int, total: int) -> None:
        self.progress.setVisible(True)
        self.btn_cancel.setVisible(True)
        if total <= 0:
            self.progress.setRange(0, 0)
        else:
            self.progress.setRange(0, 1000)
            self.progress.setValue(int(1000 * done / total))

    def end_transfer(self) -> None:
        self.progress.setVisible(False)
        self.progress.setRange(0, 1000)
        self.progress.setValue(0)
        self.btn_cancel.setVisible(False)

    def append_log(self, text: str) -> None:
        self.log.appendPlainText(text)

    def set_busy(self, busy: bool) -> None:
        for b in (self.btn_upgrade, self.btn_rollback, self.btn_refresh,
                  self.btn_browse):
            b.setEnabled(not busy)

    def set_connected(self, ok: bool) -> None:
        for b in (self.btn_upgrade, self.btn_rollback, self.btn_refresh,
                  self.btn_browse):
            b.setEnabled(ok)
        self.btn_upgrade.setEnabled(ok and self._pkg is not None)
        if not ok:
            for v in self.labels.values():
                v.setText("—")
            self.end_transfer()

    def apply_scale(self, scale: float) -> None:
        def px(base: float, minimum: int = 120) -> int:
            return max(minimum, int(round(base * scale)))

        self.file_edit.setMinimumWidth(px(360, 220))
        self.progress.setMinimumHeight(max(10, int(round(16 * scale))))
