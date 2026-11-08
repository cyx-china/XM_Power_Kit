# -*- coding: utf-8 -*-
"""
XM Power Kit V7 上位机

功能：
  1. 参数配置（读取当前/默认值、批量写入）
  2. 文件管理（浏览外部 Flash 上的 FATFS，上传/下载/删除/重命名/新建目录）
  3. 设备信息（固件版本、容量、蓝牙波特率、重启）

通信：USB CDC 虚拟串口 或 蓝牙串口（USART3，默认 115200）。两者在 Windows 上都表现为 COM 口。
协议：见 protocol.py（A5 5A + LEN + CMD + SEQ + DATA + CRC16）。
"""
from __future__ import annotations

import os
import sys
from datetime import datetime
from string import Template

from PyQt5.QtCore import Qt, QTimer
from PyQt5.QtGui import QGuiApplication
from PyQt5.QtWidgets import (
    QApplication, QComboBox, QFrame, QGridLayout, QGroupBox, QHBoxLayout,
    QLabel, QLineEdit, QMainWindow, QMessageBox, QProgressBar, QPushButton,
    QSpinBox, QTabWidget, QVBoxLayout, QWidget,
)

import params as P
from device import list_ports
from protocol import PROTO_VER, FEATURE_MAINT
from ui_files import FileBrowser
from ui_ota import OtaView
from ui_params import ParamView
from worker import Worker

BAUDS = [9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600]

# 设计基准尺寸：窗口按这个尺寸显示时缩放系数为 1.0
BASE_W, BASE_H = 1180, 760
BASE_FONT = 13
SCALE_MIN, SCALE_MAX = 0.80, 2.00

_STYLE = Template("""
QWidget { background-color: #1e1e2e; color: #cdd6f4; font-size: ${fs}px; }
QMainWindow { background-color: #181825; }
QTabWidget::pane { border: 1px solid #313244; border-radius: ${r1}px; top: -1px; }
QTabBar::tab { background: #212134; padding: ${pt}px ${pt2}px; border: 1px solid #313244;
               border-bottom: none; border-top-left-radius: ${r1}px; border-top-right-radius: ${r1}px;
               margin-right: 2px; }
QTabBar::tab:selected { background: #7aa2f7; color: #1e1e2e; font-weight: bold; }
QTabBar::tab:hover:!selected { background: #313244; }
QPushButton { background-color: #2d2d3f; border: 1px solid #3b3b54; border-radius: ${r1}px;
              padding: ${b1}px ${b2}px; }
QPushButton:hover { border: 1px solid #7aa2f7; background-color: #3b3b54; }
QPushButton:pressed { background-color: #7aa2f7; color: #1e1e2e; }
QPushButton:disabled { color: #585b70; border-color: #313244; }
QPushButton#PrimaryButton { background-color: #7aa2f7; color: #1e1e2e; font-weight: bold; }
QPushButton#PrimaryButton:hover { background-color: #89b4fa; }
QPushButton#PrimaryButton:disabled { background-color: #45475a; color: #7f849c; }
QLineEdit, QComboBox, QSpinBox { background-color: #2d2d3f; border: 1px solid #3b3b54;
                                 border-radius: ${r1}px; padding: ${i1}px ${i2}px; }
QLineEdit:focus, QComboBox:focus { border: 1px solid #7aa2f7; }
QLineEdit:read-only { background-color: #252536; color: #a6adc8; }
QTableWidget { background-color: #212134; alternate-background-color: #262638;
               gridline-color: #313244; border: 1px solid #313244; border-radius: ${r1}px; }
QHeaderView::section { background-color: #2d2d3f; color: #cdd6f4; padding: ${hp}px;
                       border: none; border-right: 1px solid #313244; }
QListWidget#NavList { background-color: #212134; border: 1px solid #313244; border-radius: ${r1}px; }
QListWidget#NavList::item { height: ${nh}px; padding-left: ${npl}px; border-radius: ${r2}px;
                            margin: ${nmv}px ${nmh}px; }
QListWidget#NavList::item:selected { background-color: #7aa2f7; color: #1e1e2e; font-weight: bold; }
QListWidget#NavList::item:hover:!selected { background-color: #313244; }
QProgressBar { border: 1px solid #313244; border-radius: ${r1}px; text-align: center;
               background-color: #2d2d3f; height: ${ph}px; }
QProgressBar::chunk { background-color: #7aa2f7; border-radius: ${r2c}px; }
QGroupBox { border: 1px solid #313244; border-radius: ${r3}px; margin-top: ${gbm}px;
            padding-top: ${gbp}px; }
QGroupBox::title { subcontrol-origin: margin; left: ${gbt}px; padding: 0 ${gbt2}px; color: #89b4fa; }
QStatusBar { background: #181825; color: #a6adc8; }
""")


def build_style(scale: float) -> str:
    """按缩放系数重建样式表（字号、内边距、控件高度一起缩放，窗口变大时整体放大）"""
    def px(base: float, minimum: int = 1) -> int:
        return max(minimum, int(round(base * scale)))

    return _STYLE.substitute(
        fs=max(10, px(BASE_FONT)),
        r1=px(6, 3), r2=px(4, 2), r3=px(8, 4), r2c=px(5, 2),
        pt=px(8), pt2=px(22),
        b1=px(6), b2=px(14),
        i1=px(5), i2=px(10),
        hp=px(6),
        nh=px(40), npl=px(14), nmv=px(3), nmh=px(6),
        ph=px(16),
        gbm=px(12), gbp=px(14), gbt=px(12), gbt2=px(6),
    )


def _log_path() -> str:
    base = os.path.dirname(sys.executable) if getattr(sys, "frozen", False) else os.path.dirname(os.path.abspath(__file__))
    return os.path.join(base, "xm_power_kit_log_%s.txt" % datetime.now().strftime("%Y%m%d"))


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("XM Power Kit V7 上位机")
        self.resize(BASE_W, BASE_H)
        self.setMinimumSize(int(BASE_W * SCALE_MIN), int(BASE_H * SCALE_MIN))

        self.current_values = None          # 最近一次读取的当前参数
        self.device_info = {}
        self.connected = False
        self.maintenance = False

        self._scale = 0.0
        self._scale_timer = QTimer(self)    # 拖拽窗口时延时重算，避免每个像素都重建样式
        self._scale_timer.setSingleShot(True)
        self._scale_timer.timeout.connect(self._apply_scale)

        self.worker = Worker()
        self.worker.start()

        self.setStyleSheet(build_style(1.0))
        self._build()
        self._wire()
        self.refresh_ports()
        self._set_connected(False)
        self._apply_scale()

    # ------------------------------------------------------------ 界面
    def _build(self) -> None:
        central = QWidget()
        root = QVBoxLayout(central)
        root.setContentsMargins(10, 10, 10, 10)
        root.setSpacing(8)

        # ---- 连接栏 ----
        conn = QFrame()
        conn.setFrameShape(QFrame.StyledPanel)
        cl = QHBoxLayout(conn)
        cl.setContentsMargins(10, 8, 10, 8)
        cl.addWidget(QLabel("通信方式"))
        self.mode_combo = QComboBox()
        self.mode_combo.addItems(["USB CDC 串口", "蓝牙串口"])
        cl.addWidget(self.mode_combo)
        cl.addSpacing(10)
        cl.addWidget(QLabel("端口"))
        self.port_combo = QComboBox()       # 端口下拉框吃掉多余宽度，窗口变宽时跟着变长
        cl.addWidget(self.port_combo, 1)
        self.btn_refresh_ports = QPushButton("刷新")
        cl.addWidget(self.btn_refresh_ports)
        cl.addSpacing(10)
        cl.addWidget(QLabel("波特率"))
        self.baud_combo = QComboBox()
        self.baud_combo.addItems([str(b) for b in BAUDS])
        self.baud_combo.setCurrentText("115200")
        cl.addWidget(self.baud_combo)
        cl.addSpacing(6)
        self.btn_connect = QPushButton("连接")
        self.btn_connect.setObjectName("PrimaryButton")
        cl.addWidget(self.btn_connect)
        cl.addSpacing(12)
        self.device_label = QLabel("未连接")
        self.device_label.setStyleSheet("color:#a6adc8;")
        cl.addWidget(self.device_label)
        root.addWidget(conn)

        # ---- 页签 ----
        self.tabs = QTabWidget()
        self.view_params = ParamView()
        self.view_files = FileBrowser()
        self.view_ota = OtaView()
        self.view_info = self._build_info_tab()
        self.tabs.addTab(self.view_params, "参数配置")
        self.tabs.addTab(self.view_files, "文件管理")
        self.tabs.addTab(self.view_ota, "固件升级")
        self.tabs.addTab(self.view_info, "设备信息")
        root.addWidget(self.tabs, 1)

        self.setCentralWidget(central)

        # ---- 状态栏 ----
        sb = self.statusBar()
        self.status_label = QLabel("就绪")
        self.status_progress = QProgressBar()
        self.status_progress.setVisible(False)
        sb.addWidget(self.status_label, 1)
        sb.addPermanentWidget(self.status_progress)

        self.mode_combo.currentIndexChanged.connect(self._on_mode_changed)

    def _build_info_tab(self) -> QWidget:
        page = QWidget()
        lay = QVBoxLayout(page)
        lay.setContentsMargins(8, 8, 8, 8)

        box = QGroupBox("设备信息")
        grid = QGridLayout(box)
        self.info_labels = {}
        rows = [
            ("firmware", "固件版本"),
            ("proto", "协议版本"),
            ("params", "参数数量"),
            ("capacity", "外部 Flash"),
            ("bt_baud", "蓝牙波特率"),
        ]
        for r, (key, title) in enumerate(rows):
            grid.addWidget(QLabel(title), r, 0)
            value = QLabel("—")
            value.setTextInteractionFlags(Qt.TextSelectableByMouse)
            grid.addWidget(value, r, 1)
            self.info_labels[key] = value
        grid.setColumnStretch(1, 1)
        lay.addWidget(box)

        bt_box = QGroupBox("蓝牙（USART3）")
        bl = QHBoxLayout(bt_box)
        bl.addWidget(QLabel("波特率"))
        self.bt_baud_combo = QComboBox()
        self.bt_baud_combo.setEditable(True)
        self.bt_baud_combo.addItems([str(b) for b in BAUDS])
        self.bt_baud_combo.setCurrentText("115200")
        bl.addWidget(self.bt_baud_combo)
        self.btn_bt_apply = QPushButton("应用到设备")
        bl.addWidget(self.btn_bt_apply)
        bl.addStretch(1)
        hint = QLabel("需与蓝牙模块一致；修改后请重新连接蓝牙。建议先用 USB 连接修改。")
        hint.setStyleSheet("color:#a6adc8;")
        bl.addWidget(hint)
        lay.addWidget(bt_box)

        op_box = QGroupBox("操作")
        ol = QHBoxLayout(op_box)
        self.btn_reboot = QPushButton("重启设备")
        self.btn_sync_fs = QPushButton("刷新字库缓存")
        ol.addWidget(self.btn_reboot)
        ol.addWidget(self.btn_sync_fs)
        ol.addStretch(1)
        tip = QLabel("上传字库后点「刷新字库缓存」可立即生效（LVGL 界面字体需重启设备）")
        tip.setStyleSheet("color:#a6adc8;")
        ol.addWidget(tip)
        lay.addWidget(op_box)
        lay.addStretch(1)
        return page

    # ------------------------------------------------------------ 信号
    def _wire(self) -> None:
        self.worker.sig_status.connect(self.status_label.setText)
        self.worker.sig_connected.connect(self._on_connected)
        self.worker.sig_info.connect(self._on_info)
        self.worker.sig_params.connect(self._on_params)
        self.worker.sig_dir.connect(self.view_files.set_entries)
        self.worker.sig_capacity.connect(self.view_files.set_capacity)
        self.worker.sig_progress.connect(self._on_progress)
        self.worker.sig_ota_info.connect(self.view_ota.set_info)
        self.worker.sig_done.connect(self._on_done)

        self.btn_refresh_ports.clicked.connect(self.refresh_ports)
        self.btn_connect.clicked.connect(self._toggle_connect)
        self.btn_bt_apply.clicked.connect(self._apply_bt_baud)
        self.btn_reboot.clicked.connect(self._reboot)
        self.btn_sync_fs.clicked.connect(lambda: self.worker.submit("sync", "info"))

        self.view_params.read_current.connect(lambda: self._read_params("current"))
        self.view_params.read_default.connect(lambda: self._read_params("default"))
        self.view_params.write_all.connect(self._write_params)

        self.view_files.request_list.connect(lambda p: self.worker.submit("list", "list", p))
        self.view_files.request_upload.connect(
            lambda local, remote: (self.view_files.set_progress(0, 0),
                                   self.worker.submit("upload", "upload", local, remote)))
        self.view_files.request_download.connect(
            lambda remote, local, size: (self.view_files.set_progress(0, size),
                                         self.worker.submit("download", "download", remote, local, size)))
        self.view_files.request_delete.connect(lambda p: self.worker.submit("delete", "delete", p))
        self.view_files.request_rename.connect(lambda a, b: self.worker.submit("rename", "rename", a, b))
        self.view_files.request_mkdir.connect(lambda p: self.worker.submit("mkdir", "mkdir", p))
        self.view_files.request_cancel.connect(self.worker.cancel_current)

        self.view_ota.request_info.connect(lambda: self.worker.submit("ota_info", "ota_info"))
        self.view_ota.request_upgrade.connect(
            lambda head, path: (self.view_ota.set_busy(True),
                                self.worker.submit("ota", "ota_upload", head, path)))
        self.view_ota.request_rollback.connect(
            lambda: (self.view_ota.set_busy(True),
                     self.worker.submit("ota_rollback", "ota_rollback")))
        self.view_ota.request_cancel.connect(self.worker.cancel_current)

    # ------------------------------------------------------------ 连接
    def refresh_ports(self) -> None:
        current = self.port_combo.currentData()
        self.port_combo.clear()
        for dev, desc in list_ports():
            self.port_combo.addItem("%s  %s" % (dev, desc), dev)
            self.port_combo.setItemData(self.port_combo.count() - 1, "%s — %s" % (dev, desc),
                                        Qt.ToolTipRole)
        if self.port_combo.count() == 0:
            self.port_combo.addItem("（未发现串口）", None)
        if current:
            idx = self.port_combo.findData(current)
            if idx >= 0:
                self.port_combo.setCurrentIndex(idx)

    def _on_mode_changed(self, index: int) -> None:
        # USB CDC 忽略波特率，蓝牙需要匹配
        self.baud_combo.setEnabled(index == 1)

    def _toggle_connect(self) -> None:
        if self.connected:
            self.worker.submit("disconnect", "disconnect")
            return
        port = self.port_combo.currentData()
        if not port:
            QMessageBox.warning(self, "提示", "没有可用串口，请检查 USB 连接或蓝牙配对")
            return
        baud = int(self.baud_combo.currentText())
        self.worker.submit("connect", "connect", port, baud)

    def _on_connected(self, ok: bool, port: str) -> None:
        self.connected = ok
        self._set_connected(ok)
        if ok:
            self.device_label.setText("已连接 %s" % port)
            # 连接后自动读取一次（维护模式没有参数，读会失败，跳过）
            if not self.maintenance:
                self._read_params("current")
                self.worker.submit("ota_info", "ota_info")
            self.tabs.setCurrentWidget(self.view_ota if self.maintenance else self.view_params)
            self.worker.submit("list", "list", "/")
        else:
            self.device_label.setText("未连接")
            self.maintenance = False

    def _set_connected(self, ok: bool) -> None:
        self.btn_connect.setText("断开" if ok else "连接")
        self.mode_combo.setEnabled(not ok)
        self.port_combo.setEnabled(not ok)
        self.baud_combo.setEnabled(not ok and self.mode_combo.currentIndex() == 1)
        self.btn_refresh_ports.setEnabled(not ok)
        self.view_params.set_busy(not ok)
        self.view_files.set_connected(ok)
        self.view_ota.set_connected(ok)
        self.btn_bt_apply.setEnabled(ok and not self.maintenance)
        for b in (self.btn_reboot, self.btn_sync_fs):
            b.setEnabled(ok)

    # ------------------------------------------------------------ 回调
    def _on_maintenance(self, maint: bool) -> None:
        """bootloader 维护模式：参数配置不可用，升级页改成“写入并安装”"""
        self.maintenance = maint
        self.view_ota.set_mode(maint)
        self.tabs.setTabEnabled(0, not maint)
        self.tabs.setTabText(0, "参数配置（维护模式）" if maint else "参数配置")
        self.btn_bt_apply.setEnabled(self.connected and not maint)
        if maint:
            self.status_label.setText("设备处于 bootloader 维护模式：可升级固件、管理文件")

    def _on_info(self, info: dict) -> None:
        self.device_info = info
        self._on_maintenance(bool(info.get("features", 0) & FEATURE_MAINT))
        self.info_labels["firmware"].setText(info.get("firmware", "—"))
        self.info_labels["proto"].setText("v%d (上位机 v%d)" % (info.get("proto", 0), PROTO_VER))
        if self.maintenance:
            self.info_labels["params"].setText("维护模式下不适用")
            self.info_labels["bt_baud"].setText("维护模式不支持")
        else:
            self.info_labels["params"].setText("%s 个 / %s 字节"
                                               % (info.get("param_count", "?"),
                                                  info.get("param_bytes", "?")))
            bt = info.get("bt_baud")
            if bt:
                self.info_labels["bt_baud"].setText(str(bt))
                self.bt_baud_combo.setCurrentText(str(bt))
        total = info.get("total_kb", 0)
        free = info.get("free_kb", 0)
        if total:
            self.info_labels["capacity"].setText("%.1f / %.1f MB 可用"
                                                 % (free / 1024.0, total / 1024.0))
            self.view_files.set_capacity(total, free)

    def _on_params(self, kind: str, values: dict) -> None:
        self.view_params.set_values(kind, values)
        if kind == "current":
            self.current_values = values

    def _on_progress(self, done: int, total: int) -> None:
        if self.tabs.currentWidget() is self.view_ota:
            self.view_ota.set_progress(done, total)
        else:
            self.view_files.set_progress(done, total)
        self.status_progress.setVisible(True)
        if total > 0:
            self.status_progress.setRange(0, 1000)
            self.status_progress.setValue(int(1000 * done / total))
        else:
            self.status_progress.setRange(0, 0)

    def _on_done(self, tag: str, ok: bool, message: str) -> None:
        self.status_progress.setVisible(False)
        self.status_progress.setRange(0, 100)
        if tag in ("upload", "download", "delete", "mkdir", "rename", "refresh_dir"):
            self.view_files.end_transfer()
        if tag in ("ota", "ota_rollback"):
            self.view_ota.end_transfer()
            self.view_ota.set_busy(False)
        if not ok:
            self.status_label.setText("失败：%s" % message)
            if tag == "ota_info":
                # 旧固件没有 OTA 命令：静默即可，不用弹窗
                return
            QMessageBox.warning(self, "操作失败", message)
            if tag == "connect" or "串口" in message or "已断开" in message:
                self.connected = False
                self._set_connected(False)
                self.device_label.setText("未连接")
            return
        if tag == "params_write":
            if self.current_values is not None:
                try:
                    self.current_values.update(self.view_params.collect_edits())
                except ValueError:
                    pass
            self.view_params.mark_written()
            QMessageBox.information(self, "完成", "参数已写入设备。\n部分设置（亮度、休眠、风扇等）需要重启设备后生效。")
        elif tag == "refresh_dir":
            if self.connected:
                self.worker.submit("list", "list", self.view_files.cwd)
        elif tag in ("upload", "download", "delete", "mkdir", "rename"):
            self.status_label.setText("完成")
        elif tag == "ota":
            self.view_ota.append_log("固件已提交，设备正在安装；稍后重新连接即可查看新版本")
            note = ("固件已上传到设备，bootloader 正在写入内部 Flash。\n\n"
                    if self.maintenance else
                    "固件已写入设备外部 Flash，设备正在重启。\n\n")
            QMessageBox.information(
                self, "升级已提交",
                note + "安装期间屏幕上会显示进度，完成后自动启动新固件。\n"
                       "重启完成后请重新连接，在「设备信息」里确认版本号。")
        elif tag == "ota_rollback":
            self.view_ota.append_log("回滚请求已提交，设备正在重启")
            QMessageBox.information(self, "回滚已提交", "设备正在重启并回滚到上一版本，请稍后重新连接。")
        elif tag == "ota_info" and self.connected:
            self.view_ota.append_log("已刷新升级状态")

    # ------------------------------------------------------------ 动作
    def _read_params(self, kind: str) -> None:
        if not self.connected:
            return
        self.worker.submit("params_" + kind, "read_params", kind)

    def _write_params(self) -> None:
        if not self.connected:
            QMessageBox.warning(self, "提示", "请先连接设备")
            return
        if self.current_values is None:
            QMessageBox.warning(self, "提示", "请先「读取当前参数」，写入会以读取到的值作为基础")
            return
        try:
            edits = self.view_params.collect_edits()
        except ValueError as exc:
            QMessageBox.warning(self, "输入有误", str(exc))
            return
        if not edits:
            QMessageBox.information(self, "提示", "没有需要写入的修改")
            return
        values = dict(self.current_values)
        values.update(edits)
        raw = P.encode_params(values)
        self.worker.submit("params_write", "write_params", raw)

    def _apply_bt_baud(self) -> None:
        try:
            baud = int(self.bt_baud_combo.currentText().strip())
        except ValueError:
            QMessageBox.warning(self, "输入有误", "波特率必须是数字")
            return
        if not 1200 <= baud <= 921600:
            QMessageBox.warning(self, "输入有误", "波特率范围 1200 ~ 921600")
            return
        if QMessageBox.question(self, "确认", "把设备蓝牙波特率改为 %d ？" % baud) == QMessageBox.Yes:
            self.worker.submit("bt_baud", "bt_baud", baud)

    def _reboot(self) -> None:
        if QMessageBox.question(self, "确认", "重启设备？当前连接会短暂断开。") == QMessageBox.Yes:
            self.worker.submit("reboot", "reboot")

    # ------------------------------------------------------------ 自适应缩放
    def resizeEvent(self, event) -> None:
        super().resizeEvent(event)
        self._scale_timer.start(120)        # 拖拽过程中只按最后一帧重算

    def _apply_scale(self) -> None:
        """按窗口大小等比例放大/缩小整套界面：字号、内边距、行高、列宽一起变"""
        scale = min(self.width() / float(BASE_W), self.height() / float(BASE_H))
        scale = max(SCALE_MIN, min(SCALE_MAX, scale))
        scale = round(scale * 40.0) / 40.0          # 0.025 一档，避免反复重建样式
        if abs(scale - self._scale) < 1e-6:
            return
        self._scale = scale
        self.setStyleSheet(build_style(scale))
        self._apply_widths(scale)
        self.view_params.apply_scale(scale)
        self.view_files.apply_scale(scale)
        self.view_ota.apply_scale(scale)

    def _apply_widths(self, scale: float) -> None:
        def px(base: float, minimum: int = 40) -> int:
            return max(minimum, int(round(base * scale)))

        self.mode_combo.setFixedWidth(px(130))
        self.btn_refresh_ports.setFixedWidth(px(60))
        self.baud_combo.setFixedWidth(px(100))
        self.status_progress.setFixedWidth(px(200))
        self.port_combo.setMinimumWidth(px(280, 160))

    def closeEvent(self, event) -> None:
        try:
            self.worker.shutdown()
        except Exception:
            pass
        event.accept()


def main() -> int:
    # 高分屏（4K / 系统缩放 150% 以上）跟随系统缩放，否则界面会小得看不清
    QApplication.setAttribute(Qt.AA_EnableHighDpiScaling, True)
    QApplication.setAttribute(Qt.AA_UseHighDpiPixmaps, True)
    QGuiApplication.setHighDpiScaleFactorRoundingPolicy(
        Qt.HighDpiScaleFactorRoundingPolicy.PassThrough)
    app = QApplication(sys.argv)
    win = MainWindow()
    win.show()
    return app.exec_()


if __name__ == "__main__":
    sys.exit(main())
