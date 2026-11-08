# -*- coding: utf-8 -*-
"""
参数表：与固件 UserCode/Manager/UserDefineManage.h 的 UserParamType_e 严格同序。

每个参数在 uparam.bin / 协议里固定占 4 字节：
    int16  / uint16 只用低 2 字节（高 2 字节为 0，小端）
    float  占满 4 字节（小端）
"""
from __future__ import annotations

import struct

# (id, 名称, 类型, 中文说明, 单位/范围)
PARAM_META = [
    (0, "DPS_Voltage_Original", "int16", "电压零点", "LSB"),
    (1, "DPS_Voltage_Factor", "float", "电压系数", "Factor"),
    (2, "DPS_Voltage_DAC_Coefficient", "float", "DAC 系数", "Coefficient"),
    (3, "DPS_Voltage_DAC_Constant", "float", "DAC 常数项", "Constant"),
    (4, "DPS_Current_Original", "int16", "电流零点", "LSB"),
    (5, "DPS_Current_Factor", "float", "电流系数", "Factor"),
    (6, "DPS_Loop_P", "float", "电流环 P", "P"),
    (7, "DPS_Loop_I", "float", "电流环 I", "I"),
    (8, "DPS_Loop_D", "float", "电流环 D", "D"),
    (9, "DPS_Fs_Voltage_1", "float", "第一组电压", "V"),
    (10, "DPS_Fs_Current_1", "float", "第一组电流", "A"),
    (11, "DPS_Fs_Voltage_2", "float", "第二组电压", "V"),
    (12, "DPS_Fs_Current_2", "float", "第二组电流", "A"),
    (13, "DPS_Fs_Voltage_3", "float", "第三组电压", "V"),
    (14, "DPS_Fs_Current_3", "float", "第三组电流", "A"),
    (15, "DPS_Fs_Voltage_4", "float", "第四组电压", "V"),
    (16, "DPS_Fs_Current_4", "float", "第四组电流", "A"),
    (17, "DPS_Fs_Voltage_5", "float", "第五组电压", "V"),
    (18, "DPS_Fs_Current_5", "float", "第五组电流", "A"),
    (19, "DPS_Fan_Start", "uint16", "风扇启转温度", "℃"),
    (20, "OSC_Original", "int16", "X1 档电压零点", "LSB"),
    (21, "OSC_Original_X2", "int16", "X2 档电压零点", "LSB"),
    (22, "OSC_Original_X4", "int16", "X4 档电压零点", "LSB"),
    (23, "OSC_Original_X8", "int16", "X8 档电压零点", "LSB"),
    (24, "OSC_Original_X16", "int16", "X16 档电压零点", "LSB"),
    (25, "OSC_Original_X32", "int16", "X32 档电压零点", "LSB"),
    (26, "OSC_Original_X64", "int16", "X64 档电压零点", "LSB"),
    (27, "OSC_Original_X128", "int16", "X128 档电压零点", "LSB"),
    (28, "OSC_Factor", "float", "输入分压比", "Factor"),
    (29, "OSC_AMP_X2", "float", "程控放大器 ×2", "Amp"),
    (30, "OSC_AMP_X4", "float", "程控放大器 ×4", "Amp"),
    (31, "OSC_AMP_X8", "float", "程控放大器 ×8", "Amp"),
    (32, "OSC_AMP_X16", "float", "程控放大器 ×16", "Amp"),
    (33, "OSC_AMP_X32", "float", "程控放大器 ×32", "Amp"),
    (34, "OSC_AMP_X64", "float", "程控放大器 ×64", "Amp"),
    (35, "OSC_AMP_X128", "float", "程控放大器 ×128", "Amp"),
    (36, "DDS_Original", "int16", "输出零点", "LSB"),
    (37, "DDS_Factor", "float", "输出倍率", "Factor"),
    (38, "DMM_Voltage_Original", "int16", "电压表零点", "LSB"),
    (39, "DMM_Voltage_Factor_B", "float", "比例系数(黑)", "Factor"),
    (40, "DMM_Voltage_Factor_R", "float", "比例系数(红)", "Factor"),
    (41, "DMM_Current_Original", "int16", "电流零点", "LSB"),
    (42, "DMM_Current_Factor", "float", "电流系数", "Factor"),
    (43, "DMM_Res_R200_Original", "int16", "200Ω 输出零点", "LSB"),
    (44, "DMM_Res_R200_Voltage", "float", "200Ω 空载电压", "V"),
    (45, "DMM_Res_R200", "float", "200Ω 基准阻值", "Ω"),
    (46, "DMM_Res_R2K_Original", "int16", "2KΩ 输出零点", "LSB"),
    (47, "DMM_Res_R2K_Voltage", "float", "2KΩ 空载电压", "V"),
    (48, "DMM_Res_R2K", "float", "2KΩ 基准阻值", "Ω"),
    (49, "Screen_Brightness", "uint16", "屏幕初始亮度", "10~100"),
    (50, "Screen_Sleeptime", "uint16", "休眠时间", "秒 0~3600"),
    (51, "Screen_ShowFlip", "uint16", "显示翻转", "0 或 1"),
    (52, "Beezer_Volume", "uint16", "蜂鸣器音量", "0~100"),
    (53, "Beezer_Time", "uint16", "蜂鸣时长", "n × 10ms"),
    (54, "Fan_Enable", "uint16", "启用风扇", "0 或 1"),
    (55, "Fan_StartTemperature", "uint16", "风扇启转温度", "20~50 ℃"),
]

PARAM_TOTAL_COUNT = len(PARAM_META)
PARAM_TOTAL_BYTES = PARAM_TOTAL_COUNT * 4

PARAM_BY_ID = {m[0]: m for m in PARAM_META}
PARAM_BY_NAME = {m[1]: m for m in PARAM_META}

# 界面分组：(页签, [(分组标题, [参数 id...]), ...])
PAGES = [
    ("数控电源", [
        ("校准参数", [0, 1, 2, 3, 4, 5]),
        ("PID 参数", [6, 7, 8]),
        ("快速设置", list(range(9, 19))),
        ("风扇", [19]),
    ]),
    ("示波器", [
        ("原点校准", list(range(20, 28))),
        ("分压校准", [28]),
        ("放大校准", list(range(29, 36))),
    ]),
    ("信号发生器", [
        ("校准参数", [36, 37]),
    ]),
    ("万用表", [
        ("电压表", [38, 39, 40]),
        ("电流表", [41, 42]),
        ("电阻表", [43, 44, 45, 46, 47, 48]),
    ]),
    ("其他配置", [
        ("屏幕", [49, 50, 51]),
        ("蜂鸣器", [52, 53]),
        ("风扇", [54, 55]),
    ]),
]


def decode_params(raw: bytes) -> dict:
    """224 字节 -> {id: 值}"""
    if len(raw) < PARAM_TOTAL_BYTES:
        raise ValueError("参数数据长度不足：%d" % len(raw))
    values = {}
    for pid, name, typ, label, unit in PARAM_META:
        chunk = raw[pid * 4: pid * 4 + 4]
        if typ == "int16":
            values[pid] = struct.unpack("<h", chunk[:2])[0]
        elif typ == "uint16":
            values[pid] = struct.unpack("<H", chunk[:2])[0]
        else:
            values[pid] = struct.unpack("<f", chunk)[0]
    return values


def encode_params(values: dict) -> bytes:
    """{id: 值} -> 224 字节（缺项按 0 填充，调用方应保证完整）"""
    buf = bytearray(PARAM_TOTAL_BYTES)
    for pid, name, typ, label, unit in PARAM_META:
        v = values.get(pid, 0)
        off = pid * 4
        if typ == "int16":
            buf[off:off + 2] = struct.pack("<h", int(round(v)))
        elif typ == "uint16":
            buf[off:off + 2] = struct.pack("<H", int(round(v)))
        else:
            buf[off:off + 4] = struct.pack("<f", float(v))
    return bytes(buf)


def format_value(pid: int, value) -> str:
    typ = PARAM_BY_ID[pid][2]
    if typ == "float":
        if value != value or value in (float("inf"), float("-inf")):
            return str(value)
        return "%.6g" % value
    return str(value)


def parse_value(pid: int, text: str):
    """把输入框文本转成数值；空串说明"不修改"，返回 None"""
    text = text.strip()
    if text == "":
        return None
    typ = PARAM_BY_ID[pid][2]
    if typ == "float":
        return float(text)
    if typ == "int16":
        v = int(text)
        if not -32768 <= v <= 32767:
            raise ValueError("超出 int16 范围")
        return v
    v = int(text)
    if not 0 <= v <= 65535:
        raise ValueError("超出 uint16 范围")
    return v
