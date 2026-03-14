/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"

extern lv_font_t * yahei_15;
extern lv_font_t * yahei_18;


void setup_scr_screen_Setting(lv_ui *ui)
{
    //Write codes screen_Setting
    ui->screen_Setting = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_Setting, 320, 240);
    lv_obj_set_scrollbar_mode(ui->screen_Setting, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_Setting, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_Setting, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Setting, lv_color_hex(0x1a1a2e), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Setting_cont_list
    ui->screen_Setting_cont_list = lv_obj_create(ui->screen_Setting);
    lv_obj_set_pos(ui->screen_Setting_cont_list, 0, 0);
    lv_obj_set_size(ui->screen_Setting_cont_list, 320, 507);
    lv_obj_set_scrollbar_mode(ui->screen_Setting_cont_list, LV_SCROLLBAR_MODE_ACTIVE);

    //Write style for screen_Setting_cont_list, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Setting_cont_list, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_cont_list, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Setting_cont_list, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Setting_cont_list, lv_color_hex(0x1a1a2e), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_cont_list, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Setting_cont_list, 36, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Setting_cont_list, 6, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Setting_cont_list, 8, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Setting_cont_list, 8, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_cont_list, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Setting_cont_sleep
    ui->screen_Setting_cont_sleep = lv_obj_create(ui->screen_Setting_cont_list);
    lv_obj_set_pos(ui->screen_Setting_cont_sleep, 0, 21);
    lv_obj_set_size(ui->screen_Setting_cont_sleep, 304, 40);
    lv_obj_set_scrollbar_mode(ui->screen_Setting_cont_sleep, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_Setting_cont_sleep, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Setting_cont_sleep, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_cont_sleep, 6, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Setting_cont_sleep, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Setting_cont_sleep, lv_color_hex(0x16213e), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_cont_sleep, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Setting_cont_sleep, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Setting_cont_sleep, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Setting_cont_sleep, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Setting_cont_sleep, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_cont_sleep, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Setting_label_9
    ui->screen_Setting_label_9 = lv_label_create(ui->screen_Setting_cont_sleep);
    lv_label_set_text(ui->screen_Setting_label_9, "自动休眠");
    lv_label_set_long_mode(ui->screen_Setting_label_9, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_Setting_label_9, 1, 12);
    lv_obj_set_size(ui->screen_Setting_label_9, 70, 16);

    //Write style for screen_Setting_label_9, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Setting_label_9, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_label_9, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_Setting_label_9, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Setting_label_9, yahei_15, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Setting_label_9, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_Setting_label_9, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_Setting_label_9, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_Setting_label_9, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Setting_label_9, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Setting_label_9, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Setting_label_9, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Setting_label_9, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Setting_label_9, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_label_9, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Setting_ddlist_SleepTime
    ui->screen_Setting_ddlist_SleepTime = lv_dropdown_create(ui->screen_Setting_cont_sleep);
    lv_dropdown_set_options(ui->screen_Setting_ddlist_SleepTime, "disable\n1min\n5min\n10min\n30min\n1h");
    lv_dropdown_set_symbol(ui->screen_Setting_ddlist_SleepTime, NULL);
    lv_obj_set_pos(ui->screen_Setting_ddlist_SleepTime, 193, 8);
    lv_obj_set_size(ui->screen_Setting_ddlist_SleepTime, 100, 24);

    //Write style for screen_Setting_ddlist_SleepTime, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_text_color(ui->screen_Setting_ddlist_SleepTime, lv_color_hex(0xeeeeee), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Setting_ddlist_SleepTime, yahei_15, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Setting_ddlist_SleepTime, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_Setting_ddlist_SleepTime, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Setting_ddlist_SleepTime, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Setting_ddlist_SleepTime, 6, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Setting_ddlist_SleepTime, 6, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_ddlist_SleepTime, 4, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Setting_ddlist_SleepTime, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Setting_ddlist_SleepTime, lv_color_hex(0x0f3460), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_ddlist_SleepTime, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_ddlist_SleepTime, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style state: LV_STATE_CHECKED for &style_screen_Setting_ddlist_SleepTime_extra_list_selected_checked
    static lv_style_t style_screen_Setting_ddlist_SleepTime_extra_list_selected_checked;
    ui_init_style(&style_screen_Setting_ddlist_SleepTime_extra_list_selected_checked);

    lv_style_set_border_width(&style_screen_Setting_ddlist_SleepTime_extra_list_selected_checked, 1);
    lv_style_set_border_opa(&style_screen_Setting_ddlist_SleepTime_extra_list_selected_checked, 255);
    lv_style_set_border_color(&style_screen_Setting_ddlist_SleepTime_extra_list_selected_checked, lv_color_hex(0xe1e6ee));
    lv_style_set_border_side(&style_screen_Setting_ddlist_SleepTime_extra_list_selected_checked, LV_BORDER_SIDE_FULL);
    lv_style_set_radius(&style_screen_Setting_ddlist_SleepTime_extra_list_selected_checked, 3);
    lv_style_set_bg_opa(&style_screen_Setting_ddlist_SleepTime_extra_list_selected_checked, 255);
    lv_style_set_bg_color(&style_screen_Setting_ddlist_SleepTime_extra_list_selected_checked, lv_color_hex(0x00a1b5));
    lv_style_set_bg_grad_dir(&style_screen_Setting_ddlist_SleepTime_extra_list_selected_checked, LV_GRAD_DIR_NONE);
    lv_obj_add_style(lv_dropdown_get_list(ui->screen_Setting_ddlist_SleepTime), &style_screen_Setting_ddlist_SleepTime_extra_list_selected_checked, LV_PART_SELECTED|LV_STATE_CHECKED);

    //Write style state: LV_STATE_DEFAULT for &style_screen_Setting_ddlist_SleepTime_extra_list_main_default
    static lv_style_t style_screen_Setting_ddlist_SleepTime_extra_list_main_default;
    ui_init_style(&style_screen_Setting_ddlist_SleepTime_extra_list_main_default);

    lv_style_set_max_height(&style_screen_Setting_ddlist_SleepTime_extra_list_main_default, 90);
    lv_style_set_text_color(&style_screen_Setting_ddlist_SleepTime_extra_list_main_default, lv_color_hex(0x0D3055));
    lv_style_set_text_font(&style_screen_Setting_ddlist_SleepTime_extra_list_main_default, &lv_font_montserratMedium_12);
    lv_style_set_text_opa(&style_screen_Setting_ddlist_SleepTime_extra_list_main_default, 255);
    lv_style_set_border_width(&style_screen_Setting_ddlist_SleepTime_extra_list_main_default, 1);
    lv_style_set_border_opa(&style_screen_Setting_ddlist_SleepTime_extra_list_main_default, 255);
    lv_style_set_border_color(&style_screen_Setting_ddlist_SleepTime_extra_list_main_default, lv_color_hex(0xe1e6ee));
    lv_style_set_border_side(&style_screen_Setting_ddlist_SleepTime_extra_list_main_default, LV_BORDER_SIDE_FULL);
    lv_style_set_radius(&style_screen_Setting_ddlist_SleepTime_extra_list_main_default, 3);
    lv_style_set_bg_opa(&style_screen_Setting_ddlist_SleepTime_extra_list_main_default, 255);
    lv_style_set_bg_color(&style_screen_Setting_ddlist_SleepTime_extra_list_main_default, lv_color_hex(0xffffff));
    lv_style_set_bg_grad_dir(&style_screen_Setting_ddlist_SleepTime_extra_list_main_default, LV_GRAD_DIR_NONE);
    lv_obj_add_style(lv_dropdown_get_list(ui->screen_Setting_ddlist_SleepTime), &style_screen_Setting_ddlist_SleepTime_extra_list_main_default, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style state: LV_STATE_DEFAULT for &style_screen_Setting_ddlist_SleepTime_extra_list_scrollbar_default
    static lv_style_t style_screen_Setting_ddlist_SleepTime_extra_list_scrollbar_default;
    ui_init_style(&style_screen_Setting_ddlist_SleepTime_extra_list_scrollbar_default);

    lv_style_set_radius(&style_screen_Setting_ddlist_SleepTime_extra_list_scrollbar_default, 3);
    lv_style_set_bg_opa(&style_screen_Setting_ddlist_SleepTime_extra_list_scrollbar_default, 255);
    lv_style_set_bg_color(&style_screen_Setting_ddlist_SleepTime_extra_list_scrollbar_default, lv_color_hex(0x00ff00));
    lv_style_set_bg_grad_dir(&style_screen_Setting_ddlist_SleepTime_extra_list_scrollbar_default, LV_GRAD_DIR_NONE);
    lv_obj_add_style(lv_dropdown_get_list(ui->screen_Setting_ddlist_SleepTime), &style_screen_Setting_ddlist_SleepTime_extra_list_scrollbar_default, LV_PART_SCROLLBAR|LV_STATE_DEFAULT);


    //Write codes screen_Setting_cont_disply
    ui->screen_Setting_cont_disply = lv_obj_create(ui->screen_Setting_cont_list);
    lv_obj_set_pos(ui->screen_Setting_cont_disply, 0, 72);
    lv_obj_set_size(ui->screen_Setting_cont_disply, 304, 40);
    lv_obj_set_scrollbar_mode(ui->screen_Setting_cont_disply, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_Setting_cont_disply, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Setting_cont_disply, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_cont_disply, 6, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Setting_cont_disply, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Setting_cont_disply, lv_color_hex(0x16213e), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_cont_disply, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Setting_cont_disply, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Setting_cont_disply, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Setting_cont_disply, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Setting_cont_disply, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_cont_disply, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Setting_label_br
    ui->screen_Setting_label_br = lv_label_create(ui->screen_Setting_cont_disply);
    lv_label_set_text(ui->screen_Setting_label_br, "屏幕亮度");
    lv_label_set_long_mode(ui->screen_Setting_label_br, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_Setting_label_br, 1, 12);
    lv_obj_set_size(ui->screen_Setting_label_br, 70, 16);

    //Write style for screen_Setting_label_br, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Setting_label_br, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_label_br, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_Setting_label_br, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Setting_label_br, yahei_15, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Setting_label_br, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_Setting_label_br, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_Setting_label_br, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_Setting_label_br, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Setting_label_br, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Setting_label_br, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Setting_label_br, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Setting_label_br, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Setting_label_br, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_label_br, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Setting_label_var
    ui->screen_Setting_label_var = lv_label_create(ui->screen_Setting_cont_disply);
    lv_label_set_text(ui->screen_Setting_label_var, "100%");
    lv_label_set_long_mode(ui->screen_Setting_label_var, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_Setting_label_var, 77, 12);
    lv_obj_set_size(ui->screen_Setting_label_var, 44, 16);

    //Write style for screen_Setting_label_var, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Setting_label_var, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_label_var, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_Setting_label_var, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Setting_label_var, yahei_15, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Setting_label_var, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_Setting_label_var, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_Setting_label_var, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_Setting_label_var, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Setting_label_var, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Setting_label_var, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Setting_label_var, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Setting_label_var, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Setting_label_var, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_label_var, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Setting_slider_ScrenLight
    ui->screen_Setting_slider_ScrenLight = lv_slider_create(ui->screen_Setting_cont_disply);
    lv_slider_set_range(ui->screen_Setting_slider_ScrenLight, 10, 100);
    lv_slider_set_mode(ui->screen_Setting_slider_ScrenLight, LV_SLIDER_MODE_NORMAL);
    lv_slider_set_value(ui->screen_Setting_slider_ScrenLight, 100, LV_ANIM_OFF);
    lv_obj_set_pos(ui->screen_Setting_slider_ScrenLight, 130, 17);
    lv_obj_set_size(ui->screen_Setting_slider_ScrenLight, 160, 6);

    //Write style for screen_Setting_slider_ScrenLight, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_Setting_slider_ScrenLight, 60, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Setting_slider_ScrenLight, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_slider_ScrenLight, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_slider_ScrenLight, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(ui->screen_Setting_slider_ScrenLight, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_slider_ScrenLight, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for screen_Setting_slider_ScrenLight, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_Setting_slider_ScrenLight, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Setting_slider_ScrenLight, lv_color_hex(0x2195f6), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_slider_ScrenLight, LV_GRAD_DIR_NONE, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_slider_ScrenLight, 8, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write style for screen_Setting_slider_ScrenLight, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_Setting_slider_ScrenLight, 255, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Setting_slider_ScrenLight, lv_color_hex(0x2195f6), LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_slider_ScrenLight, LV_GRAD_DIR_NONE, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_slider_ScrenLight, 8, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes screen_Setting_cont_direction
    ui->screen_Setting_cont_direction = lv_obj_create(ui->screen_Setting_cont_list);
    lv_obj_set_pos(ui->screen_Setting_cont_direction, 0, 123);
    lv_obj_set_size(ui->screen_Setting_cont_direction, 304, 40);
    lv_obj_set_scrollbar_mode(ui->screen_Setting_cont_direction, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_Setting_cont_direction, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Setting_cont_direction, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_cont_direction, 6, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Setting_cont_direction, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Setting_cont_direction, lv_color_hex(0x16213e), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_cont_direction, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Setting_cont_direction, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Setting_cont_direction, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Setting_cont_direction, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Setting_cont_direction, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_cont_direction, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Setting_label_1
    ui->screen_Setting_label_1 = lv_label_create(ui->screen_Setting_cont_direction);
    lv_label_set_text(ui->screen_Setting_label_1, "显示翻转");
    lv_label_set_long_mode(ui->screen_Setting_label_1, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_Setting_label_1, 1, 12);
    lv_obj_set_size(ui->screen_Setting_label_1, 70, 16);

    //Write style for screen_Setting_label_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Setting_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_Setting_label_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Setting_label_1, yahei_15, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Setting_label_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_Setting_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_Setting_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_Setting_label_1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Setting_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Setting_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Setting_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Setting_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Setting_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_label_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Setting_sw_ShowFlip
    ui->screen_Setting_sw_ShowFlip = lv_switch_create(ui->screen_Setting_cont_direction);
    lv_obj_set_pos(ui->screen_Setting_sw_ShowFlip, 219, 10);
    lv_obj_set_size(ui->screen_Setting_sw_ShowFlip, 48, 20);

    //Write style for screen_Setting_sw_ShowFlip, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_Setting_sw_ShowFlip, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Setting_sw_ShowFlip, lv_color_hex(0xe6e2e6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_sw_ShowFlip, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_Setting_sw_ShowFlip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_sw_ShowFlip, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_sw_ShowFlip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for screen_Setting_sw_ShowFlip, Part: LV_PART_INDICATOR, State: LV_STATE_CHECKED.
    lv_obj_set_style_bg_opa(ui->screen_Setting_sw_ShowFlip, 255, LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(ui->screen_Setting_sw_ShowFlip, lv_color_hex(0x2195f6), LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_sw_ShowFlip, LV_GRAD_DIR_NONE, LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_set_style_border_width(ui->screen_Setting_sw_ShowFlip, 0, LV_PART_INDICATOR|LV_STATE_CHECKED);

    //Write style for screen_Setting_sw_ShowFlip, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_Setting_sw_ShowFlip, 255, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Setting_sw_ShowFlip, lv_color_hex(0xffffff), LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_sw_ShowFlip, LV_GRAD_DIR_NONE, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_Setting_sw_ShowFlip, 0, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_sw_ShowFlip, 10, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes screen_Setting_cont_volume
    ui->screen_Setting_cont_volume = lv_obj_create(ui->screen_Setting_cont_list);
    lv_obj_set_pos(ui->screen_Setting_cont_volume, 0, 197);
    lv_obj_set_size(ui->screen_Setting_cont_volume, 304, 40);
    lv_obj_set_scrollbar_mode(ui->screen_Setting_cont_volume, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_Setting_cont_volume, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Setting_cont_volume, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_cont_volume, 6, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Setting_cont_volume, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Setting_cont_volume, lv_color_hex(0x16213e), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_cont_volume, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Setting_cont_volume, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Setting_cont_volume, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Setting_cont_volume, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Setting_cont_volume, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_cont_volume, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Setting_label_3
    ui->screen_Setting_label_3 = lv_label_create(ui->screen_Setting_cont_volume);
    lv_label_set_text(ui->screen_Setting_label_3, "蜂鸣音量");
    lv_label_set_long_mode(ui->screen_Setting_label_3, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_Setting_label_3, 1, 13);
    lv_obj_set_size(ui->screen_Setting_label_3, 70, 16);

    //Write style for screen_Setting_label_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Setting_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_Setting_label_3, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Setting_label_3, yahei_15, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Setting_label_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_Setting_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_Setting_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_Setting_label_3, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Setting_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Setting_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Setting_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Setting_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Setting_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_label_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Setting_label_2
    ui->screen_Setting_label_2 = lv_label_create(ui->screen_Setting_cont_volume);
    lv_label_set_text(ui->screen_Setting_label_2, "50%");
    lv_label_set_long_mode(ui->screen_Setting_label_2, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_Setting_label_2, 77, 12);
    lv_obj_set_size(ui->screen_Setting_label_2, 44, 16);

    //Write style for screen_Setting_label_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Setting_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_Setting_label_2, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Setting_label_2, yahei_15, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Setting_label_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_Setting_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_Setting_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_Setting_label_2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Setting_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Setting_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Setting_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Setting_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Setting_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_label_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Setting_slider_BeezerVolume
    ui->screen_Setting_slider_BeezerVolume = lv_slider_create(ui->screen_Setting_cont_volume);
    lv_slider_set_range(ui->screen_Setting_slider_BeezerVolume, 10, 100);
    lv_slider_set_mode(ui->screen_Setting_slider_BeezerVolume, LV_SLIDER_MODE_NORMAL);
    lv_slider_set_value(ui->screen_Setting_slider_BeezerVolume, 50, LV_ANIM_OFF);
    lv_obj_set_pos(ui->screen_Setting_slider_BeezerVolume, 130, 17);
    lv_obj_set_size(ui->screen_Setting_slider_BeezerVolume, 160, 6);

    //Write style for screen_Setting_slider_BeezerVolume, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_Setting_slider_BeezerVolume, 60, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Setting_slider_BeezerVolume, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_slider_BeezerVolume, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_slider_BeezerVolume, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(ui->screen_Setting_slider_BeezerVolume, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_slider_BeezerVolume, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for screen_Setting_slider_BeezerVolume, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_Setting_slider_BeezerVolume, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Setting_slider_BeezerVolume, lv_color_hex(0x2195f6), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_slider_BeezerVolume, LV_GRAD_DIR_NONE, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_slider_BeezerVolume, 8, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write style for screen_Setting_slider_BeezerVolume, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_Setting_slider_BeezerVolume, 255, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Setting_slider_BeezerVolume, lv_color_hex(0x2195f6), LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_slider_BeezerVolume, LV_GRAD_DIR_NONE, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_slider_BeezerVolume, 8, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes screen_Setting_cont_time
    ui->screen_Setting_cont_time = lv_obj_create(ui->screen_Setting_cont_list);
    lv_obj_set_pos(ui->screen_Setting_cont_time, 0, 248);
    lv_obj_set_size(ui->screen_Setting_cont_time, 304, 40);
    lv_obj_set_scrollbar_mode(ui->screen_Setting_cont_time, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_Setting_cont_time, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Setting_cont_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_cont_time, 6, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Setting_cont_time, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Setting_cont_time, lv_color_hex(0x16213e), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_cont_time, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Setting_cont_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Setting_cont_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Setting_cont_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Setting_cont_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_cont_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Setting_label_5
    ui->screen_Setting_label_5 = lv_label_create(ui->screen_Setting_cont_time);
    lv_label_set_text(ui->screen_Setting_label_5, "蜂鸣时长");
    lv_label_set_long_mode(ui->screen_Setting_label_5, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_Setting_label_5, 1, 12);
    lv_obj_set_size(ui->screen_Setting_label_5, 70, 16);

    //Write style for screen_Setting_label_5, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Setting_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_Setting_label_5, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Setting_label_5, yahei_15, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Setting_label_5, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_Setting_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_Setting_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_Setting_label_5, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Setting_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Setting_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Setting_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Setting_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Setting_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_label_5, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Setting_label_4
    ui->screen_Setting_label_4 = lv_label_create(ui->screen_Setting_cont_time);
    lv_label_set_text(ui->screen_Setting_label_4, "10ms");
    lv_label_set_long_mode(ui->screen_Setting_label_4, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_Setting_label_4, 79, 12);
    lv_obj_set_size(ui->screen_Setting_label_4, 40, 16);

    //Write style for screen_Setting_label_4, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Setting_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_Setting_label_4, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Setting_label_4, yahei_15, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Setting_label_4, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_Setting_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_Setting_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_Setting_label_4, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Setting_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Setting_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Setting_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Setting_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Setting_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_label_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Setting_slider_BeezerTime
    ui->screen_Setting_slider_BeezerTime = lv_slider_create(ui->screen_Setting_cont_time);
    lv_slider_set_range(ui->screen_Setting_slider_BeezerTime, 1, 8);
    lv_slider_set_mode(ui->screen_Setting_slider_BeezerTime, LV_SLIDER_MODE_NORMAL);
    lv_slider_set_value(ui->screen_Setting_slider_BeezerTime, 1, LV_ANIM_OFF);
    lv_obj_set_pos(ui->screen_Setting_slider_BeezerTime, 130, 17);
    lv_obj_set_size(ui->screen_Setting_slider_BeezerTime, 160, 6);

    //Write style for screen_Setting_slider_BeezerTime, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_Setting_slider_BeezerTime, 60, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Setting_slider_BeezerTime, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_slider_BeezerTime, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_slider_BeezerTime, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(ui->screen_Setting_slider_BeezerTime, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_slider_BeezerTime, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for screen_Setting_slider_BeezerTime, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_Setting_slider_BeezerTime, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Setting_slider_BeezerTime, lv_color_hex(0x2195f6), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_slider_BeezerTime, LV_GRAD_DIR_NONE, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_slider_BeezerTime, 8, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write style for screen_Setting_slider_BeezerTime, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_Setting_slider_BeezerTime, 255, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Setting_slider_BeezerTime, lv_color_hex(0x2195f6), LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_slider_BeezerTime, LV_GRAD_DIR_NONE, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_slider_BeezerTime, 8, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes screen_Setting_cont_fanon
    ui->screen_Setting_cont_fanon = lv_obj_create(ui->screen_Setting_cont_list);
    lv_obj_set_pos(ui->screen_Setting_cont_fanon, 0, 323);
    lv_obj_set_size(ui->screen_Setting_cont_fanon, 304, 40);
    lv_obj_set_scrollbar_mode(ui->screen_Setting_cont_fanon, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_Setting_cont_fanon, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Setting_cont_fanon, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_cont_fanon, 6, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Setting_cont_fanon, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Setting_cont_fanon, lv_color_hex(0x16213e), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_cont_fanon, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Setting_cont_fanon, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Setting_cont_fanon, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Setting_cont_fanon, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Setting_cont_fanon, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_cont_fanon, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Setting_label_10
    ui->screen_Setting_label_10 = lv_label_create(ui->screen_Setting_cont_fanon);
    lv_label_set_text(ui->screen_Setting_label_10, "风扇使能");
    lv_label_set_long_mode(ui->screen_Setting_label_10, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_Setting_label_10, 1, 12);
    lv_obj_set_size(ui->screen_Setting_label_10, 70, 16);

    //Write style for screen_Setting_label_10, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Setting_label_10, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_label_10, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_Setting_label_10, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Setting_label_10, yahei_15, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Setting_label_10, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_Setting_label_10, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_Setting_label_10, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_Setting_label_10, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Setting_label_10, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Setting_label_10, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Setting_label_10, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Setting_label_10, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Setting_label_10, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_label_10, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Setting_sw_FanEnable
    ui->screen_Setting_sw_FanEnable = lv_switch_create(ui->screen_Setting_cont_fanon);
    lv_obj_set_pos(ui->screen_Setting_sw_FanEnable, 219, 10);
    lv_obj_set_size(ui->screen_Setting_sw_FanEnable, 48, 20);

    //Write style for screen_Setting_sw_FanEnable, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_Setting_sw_FanEnable, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Setting_sw_FanEnable, lv_color_hex(0xe6e2e6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_sw_FanEnable, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_Setting_sw_FanEnable, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_sw_FanEnable, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_sw_FanEnable, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for screen_Setting_sw_FanEnable, Part: LV_PART_INDICATOR, State: LV_STATE_CHECKED.
    lv_obj_set_style_bg_opa(ui->screen_Setting_sw_FanEnable, 255, LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(ui->screen_Setting_sw_FanEnable, lv_color_hex(0x2195f6), LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_sw_FanEnable, LV_GRAD_DIR_NONE, LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_set_style_border_width(ui->screen_Setting_sw_FanEnable, 0, LV_PART_INDICATOR|LV_STATE_CHECKED);

    //Write style for screen_Setting_sw_FanEnable, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_Setting_sw_FanEnable, 255, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Setting_sw_FanEnable, lv_color_hex(0xffffff), LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_sw_FanEnable, LV_GRAD_DIR_NONE, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_Setting_sw_FanEnable, 0, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_sw_FanEnable, 10, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes screen_Setting_cont_fan
    ui->screen_Setting_cont_fan = lv_obj_create(ui->screen_Setting_cont_list);
    lv_obj_set_pos(ui->screen_Setting_cont_fan, 0, 374);
    lv_obj_set_size(ui->screen_Setting_cont_fan, 304, 40);
    lv_obj_set_scrollbar_mode(ui->screen_Setting_cont_fan, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_Setting_cont_fan, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Setting_cont_fan, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_cont_fan, 6, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Setting_cont_fan, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Setting_cont_fan, lv_color_hex(0x16213e), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_cont_fan, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Setting_cont_fan, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Setting_cont_fan, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Setting_cont_fan, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Setting_cont_fan, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_cont_fan, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Setting_label_7
    ui->screen_Setting_label_7 = lv_label_create(ui->screen_Setting_cont_fan);
    lv_label_set_text(ui->screen_Setting_label_7, "启转温度");
    lv_label_set_long_mode(ui->screen_Setting_label_7, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_Setting_label_7, 1, 13);
    lv_obj_set_size(ui->screen_Setting_label_7, 70, 16);

    //Write style for screen_Setting_label_7, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Setting_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_Setting_label_7, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Setting_label_7, yahei_15, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Setting_label_7, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_Setting_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_Setting_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_Setting_label_7, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Setting_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Setting_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Setting_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Setting_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Setting_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_label_7, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Setting_label_6
    ui->screen_Setting_label_6 = lv_label_create(ui->screen_Setting_cont_fan);
    lv_label_set_text(ui->screen_Setting_label_6, "40℃");
    lv_label_set_long_mode(ui->screen_Setting_label_6, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_Setting_label_6, 77, 12);
    lv_obj_set_size(ui->screen_Setting_label_6, 44, 16);

    //Write style for screen_Setting_label_6, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Setting_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_Setting_label_6, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Setting_label_6, yahei_15, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Setting_label_6, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_Setting_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_Setting_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_Setting_label_6, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Setting_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Setting_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Setting_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Setting_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Setting_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_label_6, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Setting_slider_FanStartTemp
    ui->screen_Setting_slider_FanStartTemp = lv_slider_create(ui->screen_Setting_cont_fan);
    lv_slider_set_range(ui->screen_Setting_slider_FanStartTemp, 20, 50);
    lv_slider_set_mode(ui->screen_Setting_slider_FanStartTemp, LV_SLIDER_MODE_NORMAL);
    lv_slider_set_value(ui->screen_Setting_slider_FanStartTemp, 40, LV_ANIM_OFF);
    lv_obj_set_pos(ui->screen_Setting_slider_FanStartTemp, 130, 17);
    lv_obj_set_size(ui->screen_Setting_slider_FanStartTemp, 160, 6);

    //Write style for screen_Setting_slider_FanStartTemp, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_Setting_slider_FanStartTemp, 60, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Setting_slider_FanStartTemp, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_slider_FanStartTemp, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_slider_FanStartTemp, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(ui->screen_Setting_slider_FanStartTemp, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_slider_FanStartTemp, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for screen_Setting_slider_FanStartTemp, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_Setting_slider_FanStartTemp, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Setting_slider_FanStartTemp, lv_color_hex(0x2195f6), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_slider_FanStartTemp, LV_GRAD_DIR_NONE, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_slider_FanStartTemp, 8, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write style for screen_Setting_slider_FanStartTemp, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_Setting_slider_FanStartTemp, 255, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_Setting_slider_FanStartTemp, lv_color_hex(0x2195f6), LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_Setting_slider_FanStartTemp, LV_GRAD_DIR_NONE, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_slider_FanStartTemp, 8, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes screen_Setting_btn_back
    ui->screen_Setting_btn_back = lv_btn_create(ui->screen_Setting_cont_list);
    lv_obj_add_flag(ui->screen_Setting_btn_back, LV_OBJ_FLAG_CHECKABLE);
    ui->screen_Setting_btn_back_label = lv_label_create(ui->screen_Setting_btn_back);
    lv_label_set_text(ui->screen_Setting_btn_back_label, "返回");
    lv_label_set_long_mode(ui->screen_Setting_btn_back_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_Setting_btn_back_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_Setting_btn_back, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_Setting_btn_back_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_Setting_btn_back, 85, 428);
    lv_obj_set_size(ui->screen_Setting_btn_back, 150, 22);

    //Write style for screen_Setting_btn_back, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_Setting_btn_back, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_Setting_btn_back, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_Setting_btn_back, 115, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_Setting_btn_back, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_Setting_btn_back, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_btn_back, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_btn_back, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_Setting_btn_back, lv_color_hex(0xeeeeee), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Setting_btn_back, yahei_15, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Setting_btn_back, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_Setting_btn_back, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Setting_label_group1_title
    ui->screen_Setting_label_group1_title = lv_label_create(ui->screen_Setting_cont_list);
    lv_label_set_text(ui->screen_Setting_label_group1_title, "显示设置");
    lv_label_set_long_mode(ui->screen_Setting_label_group1_title, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_Setting_label_group1_title, 2, -1);
    lv_obj_set_size(ui->screen_Setting_label_group1_title, 60, 14);

    //Write style for screen_Setting_label_group1_title, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Setting_label_group1_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_label_group1_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_Setting_label_group1_title, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Setting_label_group1_title, &lv_font_yahei_14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Setting_label_group1_title, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_Setting_label_group1_title, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_Setting_label_group1_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_Setting_label_group1_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Setting_label_group1_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Setting_label_group1_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Setting_label_group1_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Setting_label_group1_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Setting_label_group1_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_label_group1_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Setting_label_group2_title
    ui->screen_Setting_label_group2_title = lv_label_create(ui->screen_Setting_cont_list);
    lv_label_set_text(ui->screen_Setting_label_group2_title, "蜂鸣器设置");
    lv_label_set_long_mode(ui->screen_Setting_label_group2_title, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_Setting_label_group2_title, 0, 174);
    lv_obj_set_size(ui->screen_Setting_label_group2_title, 80, 14);

    //Write style for screen_Setting_label_group2_title, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Setting_label_group2_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_label_group2_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_Setting_label_group2_title, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Setting_label_group2_title, &lv_font_yahei_14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Setting_label_group2_title, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_Setting_label_group2_title, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_Setting_label_group2_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_Setting_label_group2_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Setting_label_group2_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Setting_label_group2_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Setting_label_group2_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Setting_label_group2_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Setting_label_group2_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_label_group2_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Setting_label_group3_title
    ui->screen_Setting_label_group3_title = lv_label_create(ui->screen_Setting_cont_list);
    lv_label_set_text(ui->screen_Setting_label_group3_title, "风扇设置");
    lv_label_set_long_mode(ui->screen_Setting_label_group3_title, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_Setting_label_group3_title, 2, 299);
    lv_obj_set_size(ui->screen_Setting_label_group3_title, 60, 14);

    //Write style for screen_Setting_label_group3_title, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Setting_label_group3_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_label_group3_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_Setting_label_group3_title, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Setting_label_group3_title, &lv_font_yahei_14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Setting_label_group3_title, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_Setting_label_group3_title, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_Setting_label_group3_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_Setting_label_group3_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Setting_label_group3_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Setting_label_group3_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Setting_label_group3_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Setting_label_group3_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Setting_label_group3_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_label_group3_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Setting_label_title
    ui->screen_Setting_label_title = lv_label_create(ui->screen_Setting);
    lv_label_set_text(ui->screen_Setting_label_title, "系 统 设 置");
    lv_label_set_long_mode(ui->screen_Setting_label_title, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_Setting_label_title, 110, 4);
    lv_obj_set_size(ui->screen_Setting_label_title, 100, 18);

    //Write style for screen_Setting_label_title, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_Setting_label_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Setting_label_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_Setting_label_title, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Setting_label_title, yahei_18, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Setting_label_title, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_Setting_label_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_Setting_label_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_Setting_label_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Setting_label_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Setting_label_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Setting_label_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Setting_label_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Setting_label_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Setting_label_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //The custom code of screen_Setting.


    //Update current screen layout.
    lv_obj_update_layout(ui->screen_Setting);

    //Init events for screen.
    events_init_screen_Setting(ui);
}
