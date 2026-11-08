# -*- coding: utf-8 -*-
"""参数配置页：按固件参数表自动生成表格，支持读取当前/默认值与批量写入。"""
from __future__ import annotations

from PyQt5.QtCore import Qt, pyqtSignal
from PyQt5.QtGui import QColor, QFont
from PyQt5.QtWidgets import (
    QHBoxLayout, QHeaderView, QLabel, QListWidget, QMessageBox,
    QPushButton, QStackedWidget, QTableWidget, QTableWidgetItem,
    QTabWidget, QVBoxLayout, QWidget,
)

import params as P

_COL_NAME, _COL_CUR, _COL_DEF, _COL_NEW, _COL_DESC = range(5)

_COLOR_EDIT = QColor("#f9e2af")
_COLOR_DEFAULT = QColor("#a6adc8")


class ParamView(QWidget):
    read_current = pyqtSignal()
    read_default = pyqtSignal()
    write_all = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self._cells = {}                    # pid -> {"cur": item, "def": item, "new": item}
        self._tables = []                   # 所有分组表格，供整体缩放用
        self._build()

    # ------------------------------------------------------------ 界面
    def _build(self) -> None:
        root = QVBoxLayout(self)
        root.setContentsMargins(8, 8, 8, 8)
        root.setSpacing(8)

        bar = QHBoxLayout()
        self.btn_read_cur = QPushButton("读取当前参数")
        self.btn_read_def = QPushButton("读取默认参数")
        self.btn_write = QPushButton("写入参数")
        self.btn_write.setObjectName("PrimaryButton")
        self.btn_clear = QPushButton("清空修改")
        for b in (self.btn_read_cur, self.btn_read_def, self.btn_write, self.btn_clear):
            bar.addWidget(b)
        bar.addStretch(1)
        self.hint = QLabel("双击「新值」列可修改；写入前需先读取当前参数，未填写的项保持原值")
        self.hint.setStyleSheet("color:#a6adc8;")
        bar.addWidget(self.hint)
        root.addLayout(bar)

        body = QHBoxLayout()
        body.setSpacing(8)
        self.nav = QListWidget()
        self.nav.setObjectName("NavList")
        self.nav.setFixedWidth(150)
        self.stack = QStackedWidget()
        body.addWidget(self.nav)
        body.addWidget(self.stack, 1)
        root.addLayout(body, 1)

        for page_title, groups in P.PAGES:
            self.nav.addItem(page_title)
            tabs = QTabWidget()
            for group_title, ids in groups:
                tabs.addTab(self._make_group_table(ids), group_title)
            self.stack.addWidget(tabs)
        self.nav.currentRowChanged.connect(self.stack.setCurrentIndex)
        self.nav.setCurrentRow(0)

        self.btn_read_cur.clicked.connect(self.read_current.emit)
        self.btn_read_def.clicked.connect(self.read_default.emit)
        self.btn_write.clicked.connect(self.write_all.emit)
        self.btn_clear.clicked.connect(self.clear_edits)

    def _make_group_table(self, ids) -> QWidget:
        table = QTableWidget(len(ids), 5)
        table.setHorizontalHeaderLabels(["参数", "当前值", "默认值", "新值", "说明"])
        table.verticalHeader().setVisible(False)
        table.setSelectionMode(QTableWidget.NoSelection)
        table.setEditTriggers(QTableWidget.DoubleClicked | QTableWidget.EditKeyPressed)
        table.setAlternatingRowColors(True)

        table.verticalHeader().setDefaultSectionSize(30)
        self._tables.append(table)

        hdr = table.horizontalHeader()
        hdr.setSectionResizeMode(_COL_NAME, QHeaderView.Stretch)
        hdr.setSectionResizeMode(_COL_CUR, QHeaderView.Fixed)
        hdr.setSectionResizeMode(_COL_DEF, QHeaderView.Fixed)
        hdr.setSectionResizeMode(_COL_NEW, QHeaderView.Fixed)
        hdr.setSectionResizeMode(_COL_DESC, QHeaderView.Stretch)
        table.setColumnWidth(_COL_CUR, 120)
        table.setColumnWidth(_COL_DEF, 120)
        table.setColumnWidth(_COL_NEW, 120)

        mono = QFont("Consolas")
        mono.setStyleHint(QFont.Monospace)

        for row, pid in enumerate(ids):
            _, name, typ, label, unit = P.PARAM_BY_ID[pid]
            name_item = QTableWidgetItem("%s  (%s)" % (label, typ))
            name_item.setToolTip(name)
            desc_item = QTableWidgetItem(unit)
            desc_item.setForeground(_COLOR_DEFAULT)
            cur_item = QTableWidgetItem("—")
            cur_item.setTextAlignment(Qt.AlignCenter)
            cur_item.setFont(mono)
            def_item = QTableWidgetItem("—")
            def_item.setTextAlignment(Qt.AlignCenter)
            def_item.setFont(mono)
            def_item.setForeground(_COLOR_DEFAULT)
            new_item = QTableWidgetItem("")
            new_item.setTextAlignment(Qt.AlignCenter)
            new_item.setFont(mono)
            new_item.setForeground(_COLOR_EDIT)

            for item in (name_item, desc_item, cur_item, def_item):
                item.setFlags(Qt.ItemIsEnabled)
            new_item.setFlags(Qt.ItemIsEnabled | Qt.ItemIsEditable | Qt.ItemIsSelectable)

            table.setItem(row, _COL_NAME, name_item)
            table.setItem(row, _COL_CUR, cur_item)
            table.setItem(row, _COL_DEF, def_item)
            table.setItem(row, _COL_NEW, new_item)
            table.setItem(row, _COL_DESC, desc_item)
            self._cells[pid] = {"cur": cur_item, "def": def_item, "new": new_item}

        wrap = QWidget()
        lay = QVBoxLayout(wrap)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.addWidget(table)
        return wrap

    # ------------------------------------------------------------ 数据
    def set_values(self, kind: str, values: dict) -> None:
        col = "cur" if kind == "current" else "def"
        for pid, value in values.items():
            cell = self._cells.get(pid)
            if cell is None:
                continue
            cell[col].setText(P.format_value(pid, value))
            if values.get(pid) is not None:
                cell[col].setToolTip(P.PARAM_BY_ID[pid][1])

    def clear_edits(self) -> None:
        for cell in self._cells.values():
            cell["new"].setText("")
            cell["new"].setBackground(QColor("transparent"))

    def collect_edits(self) -> dict:
        """返回 {pid: 值}；输入非法时抛 ValueError（含提示文本）"""
        edits = {}
        for pid, cell in self._cells.items():
            text = cell["new"].text().strip()
            if not text:
                continue
            old = cell["cur"].text()
            if old != "—" and text == old:
                continue                    # 没改动
            try:
                value = P.parse_value(pid, text)
            except ValueError as exc:
                raise ValueError("参数「%s」的新值「%s」无效：%s"
                                 % (P.PARAM_BY_ID[pid][3], text, exc))
            if value is not None:
                edits[pid] = value
        return edits

    def mark_written(self) -> None:
        """写入成功后把新值挪到当前值列"""
        for pid, cell in self._cells.items():
            text = cell["new"].text().strip()
            if text:
                cell["cur"].setText(text)
                cell["new"].setText("")

    def apply_scale(self, scale: float) -> None:
        """窗口大小变化时同步导航宽度、行高与数值列宽（字号由全局样式负责）"""
        def px(base: float, minimum: int = 20) -> int:
            return max(minimum, int(round(base * scale)))

        self.nav.setFixedWidth(px(150, 110))
        for table in self._tables:
            table.verticalHeader().setDefaultSectionSize(px(30, 22))
            for col in (_COL_CUR, _COL_DEF, _COL_NEW):
                table.setColumnWidth(col, px(120, 80))

    def set_busy(self, busy: bool) -> None:
        for b in (self.btn_read_cur, self.btn_read_def, self.btn_write):
            b.setEnabled(not busy)
