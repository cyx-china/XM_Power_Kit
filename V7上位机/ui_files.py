# -*- coding: utf-8 -*-
"""文件管理页：浏览外部 Flash 上的 FATFS 目录，上传/下载/删除/重命名文件。"""
from __future__ import annotations

import os

from PyQt5.QtCore import Qt, pyqtSignal
from PyQt5.QtGui import QColor, QFont
from PyQt5.QtWidgets import (
    QAbstractItemView, QFileDialog, QHBoxLayout, QHeaderView, QInputDialog,
    QLabel, QLineEdit, QMessageBox, QProgressBar, QPushButton, QTableWidget,
    QTableWidgetItem, QVBoxLayout, QWidget,
)

_COL_NAME, _COL_SIZE, _COL_TYPE = range(3)


def join_path(cwd: str, name: str) -> str:
    if cwd in ("", "/"):
        return "/" + name
    return cwd.rstrip("/") + "/" + name


def parent_path(path: str) -> str:
    path = path.rstrip("/")
    if path in ("", "/"):
        return "/"
    idx = path.rfind("/")
    return "/" if idx <= 0 else path[:idx]


def fmt_size(n: int) -> str:
    if n < 1024:
        return "%d B" % n
    if n < 1024 * 1024:
        return "%.1f KB" % (n / 1024.0)
    return "%.2f MB" % (n / 1024.0 / 1024.0)


class FileBrowser(QWidget):
    request_list = pyqtSignal(str)
    request_upload = pyqtSignal(str, str)          # 本地路径, 远端路径
    request_download = pyqtSignal(str, str, int)   # 远端路径, 本地路径, 大小
    request_delete = pyqtSignal(str)
    request_rename = pyqtSignal(str, str)
    request_mkdir = pyqtSignal(str)
    request_cancel = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.cwd = "/"
        self._entries = []
        self._build()

    # ------------------------------------------------------------ 界面
    def _build(self) -> None:
        root = QVBoxLayout(self)
        root.setContentsMargins(8, 8, 8, 8)
        root.setSpacing(8)

        bar = QHBoxLayout()
        self.btn_up = QPushButton("↑ 上级")
        self.path_edit = QLineEdit("/")
        self.path_edit.setReadOnly(True)
        self.btn_refresh = QPushButton("⟳ 刷新")
        self.btn_upload = QPushButton("上传文件")
        self.btn_upload.setObjectName("PrimaryButton")
        self.btn_download = QPushButton("下载")
        self.btn_mkdir = QPushButton("新建文件夹")
        self.btn_rename = QPushButton("重命名")
        self.btn_delete = QPushButton("删除")
        for w in (self.btn_up, self.path_edit, self.btn_refresh, self.btn_upload,
                  self.btn_download, self.btn_mkdir, self.btn_rename, self.btn_delete):
            bar.addWidget(w)
        self.path_edit.setMinimumWidth(240)
        root.addLayout(bar)

        self.table = QTableWidget(0, 3)
        self.table.setHorizontalHeaderLabels(["名称", "大小", "类型"])
        self.table.verticalHeader().setVisible(False)
        self.table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.table.setSelectionMode(QAbstractItemView.SingleSelection)
        self.table.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.table.setAlternatingRowColors(True)
        hdr = self.table.horizontalHeader()
        hdr.setSectionResizeMode(_COL_NAME, QHeaderView.Stretch)
        hdr.setSectionResizeMode(_COL_SIZE, QHeaderView.Fixed)
        hdr.setSectionResizeMode(_COL_TYPE, QHeaderView.Fixed)
        self.table.setColumnWidth(_COL_SIZE, 120)
        self.table.setColumnWidth(_COL_TYPE, 100)
        self.table.verticalHeader().setDefaultSectionSize(28)
        root.addWidget(self.table, 1)

        bottom = QHBoxLayout()
        self.capacity_label = QLabel("容量：—")
        self.capacity_label.setStyleSheet("color:#a6adc8;")
        self.transfer_label = QLabel("")
        self.progress = QProgressBar()
        self.progress.setRange(0, 100)
        self.progress.setValue(0)
        self.progress.setVisible(False)
        self.btn_cancel = QPushButton("取消")
        self.btn_cancel.setVisible(False)
        bottom.addWidget(self.capacity_label)
        bottom.addWidget(self.transfer_label, 1)
        bottom.addWidget(self.progress, 2)
        bottom.addWidget(self.btn_cancel)
        root.addLayout(bottom)

        self.btn_up.clicked.connect(self._go_up)
        self.btn_refresh.clicked.connect(lambda: self.request_list.emit(self.cwd))
        self.btn_upload.clicked.connect(self._upload)
        self.btn_download.clicked.connect(self._download)
        self.btn_mkdir.clicked.connect(self._mkdir)
        self.btn_rename.clicked.connect(self._rename)
        self.btn_delete.clicked.connect(self._delete)
        self.btn_cancel.clicked.connect(self.request_cancel.emit)
        self.table.doubleClicked.connect(self._activated)

    # ------------------------------------------------------------ 数据
    def set_entries(self, path: str, entries: list) -> None:
        self.cwd = path if path else "/"
        self._entries = entries
        self.path_edit.setText(self.cwd)
        self.table.setRowCount(len(entries))
        mono = QFont("Consolas")
        mono.setStyleHint(QFont.Monospace)
        for row, e in enumerate(entries):
            name_item = QTableWidgetItem(("📁 " if e["is_dir"] else "📄 ") + e["name"])
            name_item.setToolTip(e["name"])
            size_item = QTableWidgetItem("" if e["is_dir"] else fmt_size(e["size"]))
            size_item.setTextAlignment(Qt.AlignRight | Qt.AlignVCenter)
            size_item.setFont(mono)
            type_item = QTableWidgetItem("文件夹" if e["is_dir"] else "文件")
            type_item.setForeground(QColor("#a6adc8") if e["is_dir"] else QColor("#cdd6f4"))
            self.table.setItem(row, _COL_NAME, name_item)
            self.table.setItem(row, _COL_SIZE, size_item)
            self.table.setItem(row, _COL_TYPE, type_item)

    def set_capacity(self, total_kb: int, free_kb: int) -> None:
        if total_kb <= 0:
            self.capacity_label.setText("容量：—")
            return
        self.capacity_label.setText("容量：%.1f / %.1f MB 可用（%.0f%%）"
                                    % (free_kb / 1024.0, total_kb / 1024.0,
                                       100.0 * free_kb / total_kb))

    def set_progress(self, done: int, total: int) -> None:
        if total <= 0:
            self.progress.setVisible(True)
            self.progress.setRange(0, 0)
            self.transfer_label.setText("准备中…")
            self.btn_cancel.setVisible(True)
            return
        self.progress.setVisible(True)
        self.progress.setRange(0, 1000)
        self.progress.setValue(int(1000 * done / total))
        self.transfer_label.setText("%s / %s" % (fmt_size(done), fmt_size(total)))
        self.btn_cancel.setVisible(True)

    def end_transfer(self) -> None:
        self.progress.setVisible(False)
        self.progress.setRange(0, 100)
        self.progress.setValue(0)
        self.transfer_label.setText("")
        self.btn_cancel.setVisible(False)

    def apply_scale(self, scale: float) -> None:
        """窗口大小变化时同步列宽、行高与路径框最小宽度"""
        def px(base: float, minimum: int = 20) -> int:
            return max(minimum, int(round(base * scale)))

        self.path_edit.setMinimumWidth(px(240, 150))
        self.table.verticalHeader().setDefaultSectionSize(px(28, 20))
        self.table.setColumnWidth(_COL_SIZE, px(120, 80))
        self.table.setColumnWidth(_COL_TYPE, px(100, 70))

    def current_entry(self):
        row = self.table.currentRow()
        if 0 <= row < len(self._entries):
            return self._entries[row]
        return None

    def set_connected(self, ok: bool) -> None:
        for b in (self.btn_up, self.btn_refresh, self.btn_upload, self.btn_download,
                  self.btn_mkdir, self.btn_rename, self.btn_delete):
            b.setEnabled(ok)
        if not ok:
            self.table.setRowCount(0)
            self._entries = []
            self.cwd = "/"
            self.path_edit.setText("/")
            self.capacity_label.setText("容量：—")
            self.end_transfer()

    # ------------------------------------------------------------ 交互
    def _go_up(self) -> None:
        self.request_list.emit(parent_path(self.cwd))

    def _activated(self, index) -> None:
        e = self.current_entry()
        if e is None:
            return
        if e["is_dir"]:
            self.request_list.emit(join_path(self.cwd, e["name"]))
        else:
            self._download()

    def _upload(self) -> None:
        paths, _ = QFileDialog.getOpenFileNames(self, "选择要上传到设备外部 Flash 的文件")
        if not paths:
            return
        for p in paths:
            name = os.path.basename(p)
            self.request_upload.emit(p, join_path(self.cwd, name))

    def _download(self) -> None:
        e = self.current_entry()
        if e is None:
            QMessageBox.information(self, "提示", "请先在列表中选择一个文件")
            return
        if e["is_dir"]:
            QMessageBox.information(self, "提示", "暂不支持下载整个文件夹，请进入文件夹后逐个下载")
            return
        path, _ = QFileDialog.getSaveFileName(self, "保存到", e["name"])
        if not path:
            return
        self.request_download.emit(join_path(self.cwd, e["name"]), path, e["size"])

    def _mkdir(self) -> None:
        name, ok = QInputDialog.getText(self, "新建文件夹", "文件夹名称：")
        if not ok or not name.strip():
            return
        self.request_mkdir.emit(join_path(self.cwd, name.strip()))

    def _rename(self) -> None:
        e = self.current_entry()
        if e is None:
            QMessageBox.information(self, "提示", "请先选择要重命名的文件或文件夹")
            return
        name, ok = QInputDialog.getText(self, "重命名", "新的名称：", text=e["name"])
        if not ok or not name.strip() or name.strip() == e["name"]:
            return
        self.request_rename.emit(join_path(self.cwd, e["name"]), join_path(self.cwd, name.strip()))

    def _delete(self) -> None:
        e = self.current_entry()
        if e is None:
            QMessageBox.information(self, "提示", "请先选择要删除的文件或文件夹")
            return
        tip = "确定删除「%s」吗？" % e["name"]
        if e["is_dir"]:
            tip += "\n（文件夹必须为空才能删除）"
        if QMessageBox.question(self, "确认删除", tip) != QMessageBox.Yes:
            return
        self.request_delete.emit(join_path(self.cwd, e["name"]))
