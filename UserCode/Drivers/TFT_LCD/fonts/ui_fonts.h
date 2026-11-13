/**
  ******************************************************************************
  * @file           : ui_fonts.h
  * @brief          : 自研 GUI 用的外部字库声明（由 gen_ui_font.py 生成，见 ui_fonts.c）
  * @date           : 2026/11/13
  ******************************************************************************
  */

#ifndef XM_POWER_KIT_UI_FONTS_H
#define XM_POWER_KIT_UI_FONTS_H

#include "font.h"

extern const FontInfo ui_text;       // 16x20，中文 + ASCII，界面正文
extern const FontInfo ui_title;     // 标题 "XM Power Kit"
extern const FontInfo ui_mono;      // 9x14 等宽 ASCII，版本号 / 网址 / QQ

#endif // XM_POWER_KIT_UI_FONTS_H
