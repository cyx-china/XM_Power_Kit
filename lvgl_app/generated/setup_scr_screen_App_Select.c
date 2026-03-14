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

extern lv_font_t * acme_regular_24;

void setup_scr_screen_App_Select(lv_ui *ui)
{
    //Write codes screen_App_Select
    ui->screen_App_Select = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_App_Select, 320, 240);
    lv_obj_set_scrollbar_mode(ui->screen_App_Select, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_App_Select, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_App_Select, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_App_Select, lv_color_hex(0x1a1a2e), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_App_Select, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_App_Select_cont_1
    ui->screen_App_Select_cont_1 = lv_obj_create(ui->screen_App_Select);
    lv_obj_set_pos(ui->screen_App_Select_cont_1, 0, 0);
    lv_obj_set_size(ui->screen_App_Select_cont_1, 320, 240);
    lv_obj_set_scrollbar_mode(ui->screen_App_Select_cont_1, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_App_Select_cont_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_App_Select_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_App_Select_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_App_Select_cont_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_App_Select_cont_1, lv_color_hex(0x1a1a2e), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_App_Select_cont_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_App_Select_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_App_Select_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_App_Select_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_App_Select_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_App_Select_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_App_Select_label_Title
    ui->screen_App_Select_label_Title = lv_label_create(ui->screen_App_Select_cont_1);
    lv_label_set_text(ui->screen_App_Select_label_Title, "XM Power Kit");
    lv_label_set_long_mode(ui->screen_App_Select_label_Title, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_App_Select_label_Title, 0, 0);
    lv_obj_set_size(ui->screen_App_Select_label_Title, 320, 36);

    //Write style for screen_App_Select_label_Title, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_App_Select_label_Title, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_App_Select_label_Title, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_App_Select_label_Title, lv_color_hex(0x0f3460), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_App_Select_label_Title, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_App_Select_label_Title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_App_Select_label_Title, lv_color_hex(0x0EA5E9), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_App_Select_label_Title, acme_regular_24, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_App_Select_label_Title, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_App_Select_label_Title, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_App_Select_label_Title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_App_Select_label_Title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_App_Select_label_Title, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_App_Select_label_Title, lv_color_hex(0x16213e), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_App_Select_label_Title, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_App_Select_label_Title, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_App_Select_label_Title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_App_Select_label_Title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_App_Select_label_Title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_App_Select_label_Title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_App_Select_btn_Power
    ui->screen_App_Select_btn_Power = lv_btn_create(ui->screen_App_Select_cont_1);
    ui->screen_App_Select_btn_Power_label = lv_label_create(ui->screen_App_Select_btn_Power);
    lv_label_set_text(ui->screen_App_Select_btn_Power_label, "");
    lv_label_set_long_mode(ui->screen_App_Select_btn_Power_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_App_Select_btn_Power_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_App_Select_btn_Power, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_App_Select_btn_Power_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_App_Select_btn_Power, 10, 50);
    lv_obj_set_size(ui->screen_App_Select_btn_Power, 60, 82);

    //Write style for screen_App_Select_btn_Power, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_App_Select_btn_Power, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_App_Select_btn_Power, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_App_Select_btn_Power, 8, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_App_Select_btn_Power, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_App_Select_btn_Power, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_App_Select_btn_Power, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_App_Select_btn_Power, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_App_Select_btn_Power, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_App_Select_img_Power
    ui->screen_App_Select_img_Power = lv_img_create(ui->screen_App_Select_cont_1);
    lv_obj_add_flag(ui->screen_App_Select_img_Power, LV_OBJ_FLAG_CLICKABLE);
#if LV_USE_GUIDER_SIMULATOR
    lv_img_set_src(ui->screen_App_Select_img_Power, "F:\\LVGL\\XM_Power_Kit\\import\\image\\dy.bin");
#else
    lv_img_set_src(ui->screen_App_Select_img_Power, "S:/images/dy.bin");
#endif
    lv_img_set_pivot(ui->screen_App_Select_img_Power, 50,50);
    lv_img_set_angle(ui->screen_App_Select_img_Power, 0);
    lv_obj_set_pos(ui->screen_App_Select_img_Power, 12, 52);
    lv_obj_set_size(ui->screen_App_Select_img_Power, 56, 56);

    //Write style for screen_App_Select_img_Power, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_App_Select_img_Power, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_App_Select_img_Power, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_App_Select_img_Power, 14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_App_Select_img_Power, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_App_Select_label_Power
    ui->screen_App_Select_label_Power = lv_label_create(ui->screen_App_Select_cont_1);
    lv_label_set_text(ui->screen_App_Select_label_Power, "电源");
    lv_label_set_long_mode(ui->screen_App_Select_label_Power, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_App_Select_label_Power, 10, 114);
    lv_obj_set_size(ui->screen_App_Select_label_Power, 60, 16);

    //Write style for screen_App_Select_label_Power, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_App_Select_label_Power, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_App_Select_label_Power, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_App_Select_label_Power, lv_color_hex(0xeeeeee), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_App_Select_label_Power, &lv_font_yahei_14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_App_Select_label_Power, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_App_Select_label_Power, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_App_Select_label_Power, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_App_Select_label_Power, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_App_Select_label_Power, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_App_Select_label_Power, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_App_Select_label_Power, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_App_Select_label_Power, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_App_Select_label_Power, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_App_Select_label_Power, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_App_Select_btn_DSO
    ui->screen_App_Select_btn_DSO = lv_btn_create(ui->screen_App_Select_cont_1);
    ui->screen_App_Select_btn_DSO_label = lv_label_create(ui->screen_App_Select_btn_DSO);
    lv_label_set_text(ui->screen_App_Select_btn_DSO_label, "");
    lv_label_set_long_mode(ui->screen_App_Select_btn_DSO_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_App_Select_btn_DSO_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_App_Select_btn_DSO, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_App_Select_btn_DSO_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_App_Select_btn_DSO, 86, 50);
    lv_obj_set_size(ui->screen_App_Select_btn_DSO, 60, 82);

    //Write style for screen_App_Select_btn_DSO, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_App_Select_btn_DSO, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_App_Select_btn_DSO, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_App_Select_btn_DSO, 8, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_App_Select_btn_DSO, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_App_Select_btn_DSO, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_App_Select_btn_DSO, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_App_Select_btn_DSO, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_App_Select_btn_DSO, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_App_Select_label_DSO
    ui->screen_App_Select_label_DSO = lv_label_create(ui->screen_App_Select_cont_1);
    lv_label_set_text(ui->screen_App_Select_label_DSO, "示波器");
    lv_label_set_long_mode(ui->screen_App_Select_label_DSO, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_App_Select_label_DSO, 86, 114);
    lv_obj_set_size(ui->screen_App_Select_label_DSO, 60, 16);

    //Write style for screen_App_Select_label_DSO, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_App_Select_label_DSO, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_App_Select_label_DSO, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_App_Select_label_DSO, lv_color_hex(0xeeeeee), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_App_Select_label_DSO, &lv_font_yahei_14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_App_Select_label_DSO, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_App_Select_label_DSO, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_App_Select_label_DSO, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_App_Select_label_DSO, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_App_Select_label_DSO, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_App_Select_label_DSO, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_App_Select_label_DSO, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_App_Select_label_DSO, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_App_Select_label_DSO, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_App_Select_label_DSO, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_App_Select_img_DSO
    ui->screen_App_Select_img_DSO = lv_img_create(ui->screen_App_Select_cont_1);
    lv_obj_add_flag(ui->screen_App_Select_img_DSO, LV_OBJ_FLAG_CLICKABLE);
#if LV_USE_GUIDER_SIMULATOR
    lv_img_set_src(ui->screen_App_Select_img_DSO, "F:\\LVGL\\XM_Power_Kit\\import\\image\\sbq.bin");
#else
    lv_img_set_src(ui->screen_App_Select_img_DSO, "S:/images/sbq.bin");
#endif
    lv_img_set_pivot(ui->screen_App_Select_img_DSO, 50,50);
    lv_img_set_angle(ui->screen_App_Select_img_DSO, 0);
    lv_obj_set_pos(ui->screen_App_Select_img_DSO, 88, 52);
    lv_obj_set_size(ui->screen_App_Select_img_DSO, 56, 56);

    //Write style for screen_App_Select_img_DSO, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_App_Select_img_DSO, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_App_Select_img_DSO, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_App_Select_img_DSO, 14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_App_Select_img_DSO, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_App_Select_btn_DDS
    ui->screen_App_Select_btn_DDS = lv_btn_create(ui->screen_App_Select_cont_1);
    ui->screen_App_Select_btn_DDS_label = lv_label_create(ui->screen_App_Select_btn_DDS);
    lv_label_set_text(ui->screen_App_Select_btn_DDS_label, "");
    lv_label_set_long_mode(ui->screen_App_Select_btn_DDS_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_App_Select_btn_DDS_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_App_Select_btn_DDS, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_App_Select_btn_DDS_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_App_Select_btn_DDS, 162, 50);
    lv_obj_set_size(ui->screen_App_Select_btn_DDS, 60, 82);

    //Write style for screen_App_Select_btn_DDS, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_App_Select_btn_DDS, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_App_Select_btn_DDS, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_App_Select_btn_DDS, 8, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_App_Select_btn_DDS, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_App_Select_btn_DDS, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_App_Select_btn_DDS, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_App_Select_btn_DDS, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_App_Select_btn_DDS, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_App_Select_label_DDS
    ui->screen_App_Select_label_DDS = lv_label_create(ui->screen_App_Select_cont_1);
    lv_label_set_text(ui->screen_App_Select_label_DDS, "信号源");
    lv_label_set_long_mode(ui->screen_App_Select_label_DDS, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_App_Select_label_DDS, 162, 114);
    lv_obj_set_size(ui->screen_App_Select_label_DDS, 60, 16);

    //Write style for screen_App_Select_label_DDS, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_App_Select_label_DDS, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_App_Select_label_DDS, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_App_Select_label_DDS, lv_color_hex(0xeeeeee), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_App_Select_label_DDS, &lv_font_yahei_14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_App_Select_label_DDS, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_App_Select_label_DDS, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_App_Select_label_DDS, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_App_Select_label_DDS, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_App_Select_label_DDS, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_App_Select_label_DDS, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_App_Select_label_DDS, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_App_Select_label_DDS, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_App_Select_label_DDS, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_App_Select_label_DDS, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_App_Select_img_DDS
    ui->screen_App_Select_img_DDS = lv_img_create(ui->screen_App_Select_cont_1);
    lv_obj_add_flag(ui->screen_App_Select_img_DDS, LV_OBJ_FLAG_CLICKABLE);
#if LV_USE_GUIDER_SIMULATOR
    lv_img_set_src(ui->screen_App_Select_img_DDS, "F:\\LVGL\\XM_Power_Kit\\import\\image\\bxfsq.bin");
#else
    lv_img_set_src(ui->screen_App_Select_img_DDS, "S:/images/bxfsq.bin");
#endif
    lv_img_set_pivot(ui->screen_App_Select_img_DDS, 50,50);
    lv_img_set_angle(ui->screen_App_Select_img_DDS, 0);
    lv_obj_set_pos(ui->screen_App_Select_img_DDS, 164, 52);
    lv_obj_set_size(ui->screen_App_Select_img_DDS, 56, 56);

    //Write style for screen_App_Select_img_DDS, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_App_Select_img_DDS, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_App_Select_img_DDS, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_App_Select_img_DDS, 14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_App_Select_img_DDS, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_App_Select_btn_DMM
    ui->screen_App_Select_btn_DMM = lv_btn_create(ui->screen_App_Select_cont_1);
    ui->screen_App_Select_btn_DMM_label = lv_label_create(ui->screen_App_Select_btn_DMM);
    lv_label_set_text(ui->screen_App_Select_btn_DMM_label, "");
    lv_label_set_long_mode(ui->screen_App_Select_btn_DMM_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_App_Select_btn_DMM_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_App_Select_btn_DMM, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_App_Select_btn_DMM_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_App_Select_btn_DMM, 238, 50);
    lv_obj_set_size(ui->screen_App_Select_btn_DMM, 60, 82);

    //Write style for screen_App_Select_btn_DMM, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_App_Select_btn_DMM, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_App_Select_btn_DMM, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_App_Select_btn_DMM, 8, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_App_Select_btn_DMM, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_App_Select_btn_DMM, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_App_Select_btn_DMM, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_App_Select_btn_DMM, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_App_Select_btn_DMM, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_App_Select_label_DMM
    ui->screen_App_Select_label_DMM = lv_label_create(ui->screen_App_Select_cont_1);
    lv_label_set_text(ui->screen_App_Select_label_DMM, "万用表");
    lv_label_set_long_mode(ui->screen_App_Select_label_DMM, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_App_Select_label_DMM, 238, 114);
    lv_obj_set_size(ui->screen_App_Select_label_DMM, 60, 16);

    //Write style for screen_App_Select_label_DMM, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_App_Select_label_DMM, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_App_Select_label_DMM, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_App_Select_label_DMM, lv_color_hex(0xeeeeee), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_App_Select_label_DMM, &lv_font_yahei_14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_App_Select_label_DMM, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_App_Select_label_DMM, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_App_Select_label_DMM, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_App_Select_label_DMM, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_App_Select_label_DMM, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_App_Select_label_DMM, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_App_Select_label_DMM, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_App_Select_label_DMM, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_App_Select_label_DMM, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_App_Select_label_DMM, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_App_Select_img_DMM
    ui->screen_App_Select_img_DMM = lv_img_create(ui->screen_App_Select_cont_1);
    lv_obj_add_flag(ui->screen_App_Select_img_DMM, LV_OBJ_FLAG_CLICKABLE);
#if LV_USE_GUIDER_SIMULATOR
    lv_img_set_src(ui->screen_App_Select_img_DMM, "F:\\LVGL\\XM_Power_Kit\\import\\image\\wyb.bin");
#else
    lv_img_set_src(ui->screen_App_Select_img_DMM, "S:/images/wyb.bin");
#endif
    lv_img_set_pivot(ui->screen_App_Select_img_DMM, 50,50);
    lv_img_set_angle(ui->screen_App_Select_img_DMM, 0);
    lv_obj_set_pos(ui->screen_App_Select_img_DMM, 240, 52);
    lv_obj_set_size(ui->screen_App_Select_img_DMM, 56, 56);

    //Write style for screen_App_Select_img_DMM, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_App_Select_img_DMM, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_App_Select_img_DMM, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_App_Select_img_DMM, 14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_App_Select_img_DMM, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_App_Select_btn_SENSOR
    ui->screen_App_Select_btn_SENSOR = lv_btn_create(ui->screen_App_Select_cont_1);
    ui->screen_App_Select_btn_SENSOR_label = lv_label_create(ui->screen_App_Select_btn_SENSOR);
    lv_label_set_text(ui->screen_App_Select_btn_SENSOR_label, "");
    lv_label_set_long_mode(ui->screen_App_Select_btn_SENSOR_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_App_Select_btn_SENSOR_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_App_Select_btn_SENSOR, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_App_Select_btn_SENSOR_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_App_Select_btn_SENSOR, 10, 143);
    lv_obj_set_size(ui->screen_App_Select_btn_SENSOR, 60, 82);

    //Write style for screen_App_Select_btn_SENSOR, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_App_Select_btn_SENSOR, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_App_Select_btn_SENSOR, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_App_Select_btn_SENSOR, 8, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_App_Select_btn_SENSOR, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_App_Select_btn_SENSOR, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_App_Select_btn_SENSOR, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_App_Select_btn_SENSOR, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_App_Select_btn_SENSOR, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_App_Select_label_SENSOR
    ui->screen_App_Select_label_SENSOR = lv_label_create(ui->screen_App_Select_cont_1);
    lv_label_set_text(ui->screen_App_Select_label_SENSOR, "传感器");
    lv_label_set_long_mode(ui->screen_App_Select_label_SENSOR, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_App_Select_label_SENSOR, 10, 206);
    lv_obj_set_size(ui->screen_App_Select_label_SENSOR, 60, 16);

    //Write style for screen_App_Select_label_SENSOR, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_App_Select_label_SENSOR, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_App_Select_label_SENSOR, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_App_Select_label_SENSOR, lv_color_hex(0xeeeeee), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_App_Select_label_SENSOR, &lv_font_yahei_14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_App_Select_label_SENSOR, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_App_Select_label_SENSOR, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_App_Select_label_SENSOR, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_App_Select_label_SENSOR, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_App_Select_label_SENSOR, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_App_Select_label_SENSOR, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_App_Select_label_SENSOR, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_App_Select_label_SENSOR, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_App_Select_label_SENSOR, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_App_Select_label_SENSOR, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_App_Select_img_SENSOR
    ui->screen_App_Select_img_SENSOR = lv_img_create(ui->screen_App_Select_cont_1);
    lv_obj_add_flag(ui->screen_App_Select_img_SENSOR, LV_OBJ_FLAG_CLICKABLE);
#if LV_USE_GUIDER_SIMULATOR
    lv_img_set_src(ui->screen_App_Select_img_SENSOR, "F:\\LVGL\\XM_Power_Kit\\import\\image\\cgq.bin");
#else
    lv_img_set_src(ui->screen_App_Select_img_SENSOR, "S:/images/cgq.bin");
#endif
    lv_img_set_pivot(ui->screen_App_Select_img_SENSOR, 50,50);
    lv_img_set_angle(ui->screen_App_Select_img_SENSOR, 0);
    lv_obj_set_pos(ui->screen_App_Select_img_SENSOR, 12, 145);
    lv_obj_set_size(ui->screen_App_Select_img_SENSOR, 56, 56);

    //Write style for screen_App_Select_img_SENSOR, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_App_Select_img_SENSOR, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_App_Select_img_SENSOR, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_App_Select_img_SENSOR, 14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_App_Select_img_SENSOR, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_App_Select_btn_CLBAT
    ui->screen_App_Select_btn_CLBAT = lv_btn_create(ui->screen_App_Select_cont_1);
    ui->screen_App_Select_btn_CLBAT_label = lv_label_create(ui->screen_App_Select_btn_CLBAT);
    lv_label_set_text(ui->screen_App_Select_btn_CLBAT_label, "");
    lv_label_set_long_mode(ui->screen_App_Select_btn_CLBAT_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_App_Select_btn_CLBAT_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_App_Select_btn_CLBAT, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_App_Select_btn_CLBAT_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_App_Select_btn_CLBAT, 86, 143);
    lv_obj_set_size(ui->screen_App_Select_btn_CLBAT, 60, 82);

    //Write style for screen_App_Select_btn_CLBAT, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_App_Select_btn_CLBAT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_App_Select_btn_CLBAT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_App_Select_btn_CLBAT, 8, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_App_Select_btn_CLBAT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_App_Select_btn_CLBAT, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_App_Select_btn_CLBAT, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_App_Select_btn_CLBAT, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_App_Select_btn_CLBAT, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_App_Select_label_CLBAT
    ui->screen_App_Select_label_CLBAT = lv_label_create(ui->screen_App_Select_cont_1);
    lv_label_set_text(ui->screen_App_Select_label_CLBAT, "校准");
    lv_label_set_long_mode(ui->screen_App_Select_label_CLBAT, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_App_Select_label_CLBAT, 86, 206);
    lv_obj_set_size(ui->screen_App_Select_label_CLBAT, 60, 16);

    //Write style for screen_App_Select_label_CLBAT, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_App_Select_label_CLBAT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_App_Select_label_CLBAT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_App_Select_label_CLBAT, lv_color_hex(0xeeeeee), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_App_Select_label_CLBAT, &lv_font_yahei_14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_App_Select_label_CLBAT, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_App_Select_label_CLBAT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_App_Select_label_CLBAT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_App_Select_label_CLBAT, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_App_Select_label_CLBAT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_App_Select_label_CLBAT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_App_Select_label_CLBAT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_App_Select_label_CLBAT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_App_Select_label_CLBAT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_App_Select_label_CLBAT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_App_Select_img_CLBAT
    ui->screen_App_Select_img_CLBAT = lv_img_create(ui->screen_App_Select_cont_1);
    lv_obj_add_flag(ui->screen_App_Select_img_CLBAT, LV_OBJ_FLAG_CLICKABLE);
#if LV_USE_GUIDER_SIMULATOR
    lv_img_set_src(ui->screen_App_Select_img_CLBAT, "F:\\LVGL\\XM_Power_Kit\\import\\image\\jiaozhun.bin");
#else
    lv_img_set_src(ui->screen_App_Select_img_CLBAT, "S:/images/jiaozhun.bin");
#endif
    lv_img_set_pivot(ui->screen_App_Select_img_CLBAT, 50,50);
    lv_img_set_angle(ui->screen_App_Select_img_CLBAT, 0);
    lv_obj_set_pos(ui->screen_App_Select_img_CLBAT, 88, 145);
    lv_obj_set_size(ui->screen_App_Select_img_CLBAT, 56, 56);

    //Write style for screen_App_Select_img_CLBAT, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_App_Select_img_CLBAT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_App_Select_img_CLBAT, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_App_Select_img_CLBAT, 14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_App_Select_img_CLBAT, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_App_Select_btn_SET
    ui->screen_App_Select_btn_SET = lv_btn_create(ui->screen_App_Select_cont_1);
    ui->screen_App_Select_btn_SET_label = lv_label_create(ui->screen_App_Select_btn_SET);
    lv_label_set_text(ui->screen_App_Select_btn_SET_label, "");
    lv_label_set_long_mode(ui->screen_App_Select_btn_SET_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_App_Select_btn_SET_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_App_Select_btn_SET, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_App_Select_btn_SET_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_App_Select_btn_SET, 162, 143);
    lv_obj_set_size(ui->screen_App_Select_btn_SET, 60, 82);

    //Write style for screen_App_Select_btn_SET, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_App_Select_btn_SET, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_App_Select_btn_SET, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_App_Select_btn_SET, 8, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_App_Select_btn_SET, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_App_Select_btn_SET, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_App_Select_btn_SET, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_App_Select_btn_SET, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_App_Select_btn_SET, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_App_Select_label_SET
    ui->screen_App_Select_label_SET = lv_label_create(ui->screen_App_Select_cont_1);
    lv_label_set_text(ui->screen_App_Select_label_SET, "设置");
    lv_label_set_long_mode(ui->screen_App_Select_label_SET, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_App_Select_label_SET, 162, 206);
    lv_obj_set_size(ui->screen_App_Select_label_SET, 60, 16);

    //Write style for screen_App_Select_label_SET, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_App_Select_label_SET, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_App_Select_label_SET, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_App_Select_label_SET, lv_color_hex(0xeeeeee), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_App_Select_label_SET, &lv_font_yahei_14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_App_Select_label_SET, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_App_Select_label_SET, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_App_Select_label_SET, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_App_Select_label_SET, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_App_Select_label_SET, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_App_Select_label_SET, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_App_Select_label_SET, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_App_Select_label_SET, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_App_Select_label_SET, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_App_Select_label_SET, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_App_Select_img_SET
    ui->screen_App_Select_img_SET = lv_img_create(ui->screen_App_Select_cont_1);
    lv_obj_add_flag(ui->screen_App_Select_img_SET, LV_OBJ_FLAG_CLICKABLE);
#if LV_USE_GUIDER_SIMULATOR
    lv_img_set_src(ui->screen_App_Select_img_SET, "F:\\LVGL\\XM_Power_Kit\\import\\image\\sz.bin");
#else
    lv_img_set_src(ui->screen_App_Select_img_SET, "S:/images/sz.bin");
#endif
    lv_img_set_pivot(ui->screen_App_Select_img_SET, 50,50);
    lv_img_set_angle(ui->screen_App_Select_img_SET, 0);
    lv_obj_set_pos(ui->screen_App_Select_img_SET, 164, 145);
    lv_obj_set_size(ui->screen_App_Select_img_SET, 56, 56);

    //Write style for screen_App_Select_img_SET, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_App_Select_img_SET, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_App_Select_img_SET, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_App_Select_img_SET, 14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_App_Select_img_SET, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_App_Select_btn_ABOUT
    ui->screen_App_Select_btn_ABOUT = lv_btn_create(ui->screen_App_Select_cont_1);
    ui->screen_App_Select_btn_ABOUT_label = lv_label_create(ui->screen_App_Select_btn_ABOUT);
    lv_label_set_text(ui->screen_App_Select_btn_ABOUT_label, "");
    lv_label_set_long_mode(ui->screen_App_Select_btn_ABOUT_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_App_Select_btn_ABOUT_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_App_Select_btn_ABOUT, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_App_Select_btn_ABOUT_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_App_Select_btn_ABOUT, 238, 143);
    lv_obj_set_size(ui->screen_App_Select_btn_ABOUT, 60, 82);

    //Write style for screen_App_Select_btn_ABOUT, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_App_Select_btn_ABOUT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_App_Select_btn_ABOUT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_App_Select_btn_ABOUT, 8, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_App_Select_btn_ABOUT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_App_Select_btn_ABOUT, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_App_Select_btn_ABOUT, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_App_Select_btn_ABOUT, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_App_Select_btn_ABOUT, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_App_Select_label_ABOUT
    ui->screen_App_Select_label_ABOUT = lv_label_create(ui->screen_App_Select_cont_1);
    lv_label_set_text(ui->screen_App_Select_label_ABOUT, "关于");
    lv_label_set_long_mode(ui->screen_App_Select_label_ABOUT, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_App_Select_label_ABOUT, 238, 206);
    lv_obj_set_size(ui->screen_App_Select_label_ABOUT, 60, 16);

    //Write style for screen_App_Select_label_ABOUT, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_App_Select_label_ABOUT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_App_Select_label_ABOUT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_App_Select_label_ABOUT, lv_color_hex(0xeeeeee), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_App_Select_label_ABOUT, &lv_font_yahei_14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_App_Select_label_ABOUT, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_App_Select_label_ABOUT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_App_Select_label_ABOUT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_App_Select_label_ABOUT, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_App_Select_label_ABOUT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_App_Select_label_ABOUT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_App_Select_label_ABOUT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_App_Select_label_ABOUT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_App_Select_label_ABOUT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_App_Select_label_ABOUT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_App_Select_img_ABOUT
    ui->screen_App_Select_img_ABOUT = lv_img_create(ui->screen_App_Select_cont_1);
    lv_obj_add_flag(ui->screen_App_Select_img_ABOUT, LV_OBJ_FLAG_CLICKABLE);
#if LV_USE_GUIDER_SIMULATOR
    lv_img_set_src(ui->screen_App_Select_img_ABOUT, "F:\\LVGL\\XM_Power_Kit\\import\\image\\xinxi.bin");
#else
    lv_img_set_src(ui->screen_App_Select_img_ABOUT, "S:/images/xinxi.bin");
#endif
    lv_img_set_pivot(ui->screen_App_Select_img_ABOUT, 50,50);
    lv_img_set_angle(ui->screen_App_Select_img_ABOUT, 0);
    lv_obj_set_pos(ui->screen_App_Select_img_ABOUT, 240, 145);
    lv_obj_set_size(ui->screen_App_Select_img_ABOUT, 56, 56);

    //Write style for screen_App_Select_img_ABOUT, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_App_Select_img_ABOUT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_App_Select_img_ABOUT, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_App_Select_img_ABOUT, 14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_App_Select_img_ABOUT, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //The custom code of screen_App_Select.


    //Update current screen layout.
    lv_obj_update_layout(ui->screen_App_Select);

    //Init events for screen.
    events_init_screen_App_Select(ui);
}
