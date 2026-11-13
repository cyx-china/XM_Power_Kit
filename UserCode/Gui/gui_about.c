/**
  ******************************************************************************
  * @file           : gui_about.c
  * @brief          : 关于界面：标题、固件版本、作者信息
  * @date           : 2026/11/13
  ******************************************************************************
  */

#include "gui.h"
#include "UserTask.h"
#include "cmsis_os2.h"

extern char Sys_Version[30];        // 固件版本串，main.c 里初始化

static void about_draw(void) {
    Gui_Clear(GUI_COL_BG);

    Gui_TextCenter(160, 18, "XM Power Kit", &ui_title, GUI_COL_TITLE, GUI_COL_BG);
    Gui_Line(20, 52, 299, 52, GUI_COL_CARD);

    /* 版本号胶囊 */
    Gui_RoundFill(20, 62, 299, 90, 8, GUI_COL_CARD);
    Gui_TextCenter(160, 69, Sys_Version, &ui_mono, GUI_COL_TEXT, GUI_COL_CARD);

    /* 两行做左右两列对齐：标签固定 x，内容固定 x */
    Gui_Text(56, 104, "开源地址", &ui_text, GUI_COL_TEXT_DIM, GUI_COL_BG);
    Gui_Text(152, 104, "xuemeng.work", &ui_mono, GUI_COL_TEXT_SUB, GUI_COL_BG);
    Gui_Text(56, 128, "交流群", &ui_text, GUI_COL_TEXT_DIM, GUI_COL_BG);
    Gui_Text(152, 128, "984946410", &ui_mono, GUI_COL_TEXT_SUB, GUI_COL_BG);

    Gui_Line(20, 160, 299, 160, GUI_COL_CARD);
    Gui_TextCenter(160, 172, "Copyright (C) Xuemeng", &ui_mono, GUI_COL_TEXT_DIM, GUI_COL_BG);
    Gui_TextCenter(160, 190, "CC-BY-NC-SA 4.0", &ui_mono, GUI_COL_TEXT_DIM, GUI_COL_BG);

    Gui_TextCenter(160, 216, "↓ 设备信息    SET 返回", &ui_text, GUI_COL_TEXT_DIM, GUI_COL_BG);
}

static void about_on_key(GuiKey_t key) {
    switch (key) {
        case GUI_KEY_DOWN:
        case GUI_KEY_RIGHT:
            Gui_Navigate(&gui_screen_diag);      // 下键/编码器右转进入设备信息
            break;
        case GUI_KEY_ENTER:
        case GUI_KEY_BACK:
        case GUI_KEY_UP:
        case GUI_KEY_LEFT:
            Gui_Back();
            break;
        default:
            break;
    }
}

static void about_on_enter(void) {
    about_draw();
}

const GuiScreen gui_screen_about = {
    .on_enter = about_on_enter,
    .on_exit  = NULL,
    .on_key   = about_on_key,
    .on_tick  = NULL,
    .on_draw  = about_draw
};
