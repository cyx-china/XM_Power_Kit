/**
  ******************************************************************************
  * @file           : gui_home.c
  * @brief          : 主界面（APP 选择）：2 行 4 列图标网格 + 焦点框
  * @date           : 2026/11/13
  ******************************************************************************
  * 布局沿用原 LVGL 界面：标题栏 y=0..35，图标 56x56，卡片 60x82，
  * 第 1 行 y=50、第 2 行 y=143，列起点 10/86/162/238。
  ******************************************************************************
  */

#include "gui.h"
#include "cmsis_os2.h"
#include "os_handles.h"
#include "UserTask.h"
#include "SwitchManager.h"

#define HOME_COL_X0      10U
#define HOME_ROW_Y0      50U
#define HOME_COL_STEP    76U
#define HOME_ROW_STEP    93U
#define HOME_TILE_W      60U
#define HOME_TILE_H      82U
#define HOME_ICON_IN_X   2U
#define HOME_ICON_IN_Y   2U
#define HOME_LABEL_DY    64U
#define HOME_TILE_NUM    8U

typedef enum {
    HOME_ACTION_APP = 0,        // 切换 APP
    HOME_ACTION_SETTINGS,       // 设置页
    HOME_ACTION_ABOUT           // 关于页
} HomeAction_t;

typedef struct {
    const ImageInfo *icon;
    const char *label;
    HomeAction_t action;
    AppListType app;            // action == HOME_ACTION_APP 时有效
} HomeTile_t;

static const HomeTile_t s_tiles[HOME_TILE_NUM] = {
    { &img_dy_56x56,       "电源",   HOME_ACTION_APP,      APP_POWER },
    { &img_sbq_56x56,      "示波器", HOME_ACTION_APP,      APP_DSO   },
    { &img_bxfsq_56x56,    "信号源", HOME_ACTION_APP,      APP_AWG   },
    { &img_wyb_56x56,      "万用表", HOME_ACTION_APP,      APP_DMM   },
    { &img_pd_56x56,       "PD",     HOME_ACTION_APP,      APP_PD    },   // USB PD 协议检测 / 诱骗
    { &img_jiaozhun_56x56, "校准",   HOME_ACTION_APP,      APP_CAL   },
    { &img_sz_56x56,       "设置",   HOME_ACTION_SETTINGS, APP_NONE  },
    { &img_xinxi_56x56,    "关于",   HOME_ACTION_ABOUT,    APP_NONE  }
};

static uint8_t s_cursor = 0;

static uint16_t home_tile_x(uint8_t index) {
    return (uint16_t) (HOME_COL_X0 + (index % 4U) * HOME_COL_STEP);
}

static uint16_t home_tile_y(uint8_t index) {
    return (uint16_t) (HOME_ROW_Y0 + (index / 4U) * HOME_ROW_STEP);
}

/* 画一个卡片：先擦掉整块（含焦点框），再画图标与文字 */
static void home_draw_tile(uint8_t index, bool focused) {
    const HomeTile_t *tile = &s_tiles[index];
    uint16_t x = home_tile_x(index);
    uint16_t y = home_tile_y(index);

    Gui_Fill((uint16_t) (x - 3U), (uint16_t) (y - 3U),
             (uint16_t) (x + HOME_TILE_W + 2U), (uint16_t) (y + HOME_TILE_H + 2U), GUI_COL_BG);

    (void) lcd_draw_image((uint16_t) (x + HOME_ICON_IN_X), (uint16_t) (y + HOME_ICON_IN_Y), tile->icon);
    Gui_TextCenter((uint16_t) (x + HOME_TILE_W / 2U), (uint16_t) (y + HOME_LABEL_DY), tile->label,
                   &ui_text, focused ? GUI_COL_ACCENT : GUI_COL_TEXT_SUB, GUI_COL_BG);

    if (focused) {
        Gui_RoundFrame((uint16_t) (x - 2U), (uint16_t) (y - 2U),
                       (uint16_t) (x + HOME_TILE_W + 1U), (uint16_t) (y + HOME_TILE_H + 1U),
                       10, GUI_COL_ACCENT, 2);
    }
}

static void home_draw(void) {
    uint8_t i;

    Gui_Clear(GUI_COL_BG);
    Gui_TextCenter(160, 5, "XM Power Kit", &ui_title, GUI_COL_TITLE, GUI_COL_BG);
    Gui_Line(0, 34, 319, 34, GUI_COL_CARD);

    for (i = 0; i < HOME_TILE_NUM; i++) {
        home_draw_tile(i, i == s_cursor);
    }
}

static void home_activate(void) {
    const HomeTile_t *tile = &s_tiles[s_cursor];

    switch (tile->action) {
        case HOME_ACTION_SETTINGS:
            Gui_Navigate(&gui_screen_settings);
            break;
        case HOME_ACTION_ABOUT:
            Gui_Navigate(&gui_screen_about);
            break;
        case HOME_ACTION_APP:
        default:
            if (tile->app != APP_NONE) {
                AppListType app = tile->app;
                (void) osMessageQueuePut(AppSwitchQueueHandle, &app, 0, 10);
            }
            break;
    }
}

static void home_move(int8_t delta) {
    uint8_t old = s_cursor;
    int16_t next = (int16_t) s_cursor + delta;

    while (next < 0) {
        next = (int16_t) (next + HOME_TILE_NUM);
    }
    next = (int16_t) (next % HOME_TILE_NUM);
    if ((uint8_t) next == old) {
        return;
    }
    s_cursor = (uint8_t) next;
    home_draw_tile(old, false);
    home_draw_tile(s_cursor, true);
}

static void home_on_key(GuiKey_t key) {
    switch (key) {
        case GUI_KEY_UP:
        case GUI_KEY_LEFT:
            home_move(-1);
            break;
        case GUI_KEY_DOWN:
        case GUI_KEY_RIGHT:
            home_move(1);
            break;
        case GUI_KEY_ENTER:
            home_activate();
            break;
        case GUI_KEY_BACK:
        default:
            break;
    }
}

static void home_on_enter(void) {
    if (s_cursor >= HOME_TILE_NUM) {        // 从设置/关于返回时保留原来的焦点位置
        s_cursor = 0;
    }
    home_draw();
}

const GuiScreen gui_screen_home = {
    .on_enter = home_on_enter,
    .on_exit  = NULL,
    .on_key   = home_on_key,
    .on_draw  = home_draw
};
