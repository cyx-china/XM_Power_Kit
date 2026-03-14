/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#ifndef GUI_GUIDER_H
#define GUI_GUIDER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

typedef struct
{
  
	lv_obj_t *screen_App_Select;
	bool screen_App_Select_del;
	lv_obj_t *screen_App_Select_cont_1;
	lv_obj_t *screen_App_Select_label_Title;
	lv_obj_t *screen_App_Select_btn_Power;
	lv_obj_t *screen_App_Select_btn_Power_label;
	lv_obj_t *screen_App_Select_img_Power;
	lv_obj_t *screen_App_Select_label_Power;
	lv_obj_t *screen_App_Select_btn_DSO;
	lv_obj_t *screen_App_Select_btn_DSO_label;
	lv_obj_t *screen_App_Select_label_DSO;
	lv_obj_t *screen_App_Select_img_DSO;
	lv_obj_t *screen_App_Select_btn_DDS;
	lv_obj_t *screen_App_Select_btn_DDS_label;
	lv_obj_t *screen_App_Select_label_DDS;
	lv_obj_t *screen_App_Select_img_DDS;
	lv_obj_t *screen_App_Select_btn_DMM;
	lv_obj_t *screen_App_Select_btn_DMM_label;
	lv_obj_t *screen_App_Select_label_DMM;
	lv_obj_t *screen_App_Select_img_DMM;
	lv_obj_t *screen_App_Select_btn_SENSOR;
	lv_obj_t *screen_App_Select_btn_SENSOR_label;
	lv_obj_t *screen_App_Select_label_SENSOR;
	lv_obj_t *screen_App_Select_img_SENSOR;
	lv_obj_t *screen_App_Select_btn_CLBAT;
	lv_obj_t *screen_App_Select_btn_CLBAT_label;
	lv_obj_t *screen_App_Select_label_CLBAT;
	lv_obj_t *screen_App_Select_img_CLBAT;
	lv_obj_t *screen_App_Select_btn_SET;
	lv_obj_t *screen_App_Select_btn_SET_label;
	lv_obj_t *screen_App_Select_label_SET;
	lv_obj_t *screen_App_Select_img_SET;
	lv_obj_t *screen_App_Select_btn_ABOUT;
	lv_obj_t *screen_App_Select_btn_ABOUT_label;
	lv_obj_t *screen_App_Select_label_ABOUT;
	lv_obj_t *screen_App_Select_img_ABOUT;
	lv_obj_t *screen_Setting;
	bool screen_Setting_del;
	lv_obj_t *screen_Setting_cont_list;
	lv_obj_t *screen_Setting_cont_sleep;
	lv_obj_t *screen_Setting_label_9;
	lv_obj_t *screen_Setting_ddlist_SleepTime;
	lv_obj_t *screen_Setting_cont_disply;
	lv_obj_t *screen_Setting_label_br;
	lv_obj_t *screen_Setting_label_var;
	lv_obj_t *screen_Setting_slider_ScrenLight;
	lv_obj_t *screen_Setting_cont_direction;
	lv_obj_t *screen_Setting_label_1;
	lv_obj_t *screen_Setting_sw_ShowFlip;
	lv_obj_t *screen_Setting_cont_volume;
	lv_obj_t *screen_Setting_label_3;
	lv_obj_t *screen_Setting_label_2;
	lv_obj_t *screen_Setting_slider_BeezerVolume;
	lv_obj_t *screen_Setting_cont_time;
	lv_obj_t *screen_Setting_label_5;
	lv_obj_t *screen_Setting_label_4;
	lv_obj_t *screen_Setting_slider_BeezerTime;
	lv_obj_t *screen_Setting_cont_fanon;
	lv_obj_t *screen_Setting_label_10;
	lv_obj_t *screen_Setting_sw_FanEnable;
	lv_obj_t *screen_Setting_cont_fan;
	lv_obj_t *screen_Setting_label_7;
	lv_obj_t *screen_Setting_label_6;
	lv_obj_t *screen_Setting_slider_FanStartTemp;
	lv_obj_t *screen_Setting_btn_back;
	lv_obj_t *screen_Setting_btn_back_label;
	lv_obj_t *screen_Setting_label_group1_title;
	lv_obj_t *screen_Setting_label_group2_title;
	lv_obj_t *screen_Setting_label_group3_title;
	lv_obj_t *screen_Setting_label_title;
	lv_obj_t *screen_info;
	bool screen_info_del;
	lv_obj_t *screen_info_label_Version;
	lv_obj_t *screen_info_label_1;
	lv_obj_t *screen_info_label_2;
	lv_obj_t *screen_info_label_3;
	lv_obj_t *screen_info_label_4;
	lv_obj_t *screen_info_btn_1;
	lv_obj_t *screen_info_btn_1_label;
}lv_ui;

typedef void (*ui_setup_scr_t)(lv_ui * ui);

void ui_init_style(lv_style_t * style);

void ui_load_scr_animation(lv_ui *ui, lv_obj_t ** new_scr, bool new_scr_del, bool * old_scr_del, ui_setup_scr_t setup_scr,
                           lv_scr_load_anim_t anim_type, uint32_t time, uint32_t delay, bool is_clean, bool auto_del);

void ui_animation(void * var, int32_t duration, int32_t delay, int32_t start_value, int32_t end_value, lv_anim_path_cb_t path_cb,
                       uint16_t repeat_cnt, uint32_t repeat_delay, uint32_t playback_time, uint32_t playback_delay,
                       lv_anim_exec_xcb_t exec_cb, lv_anim_start_cb_t start_cb, lv_anim_ready_cb_t ready_cb, lv_anim_deleted_cb_t deleted_cb);


void init_scr_del_flag(lv_ui *ui);

void setup_ui(lv_ui *ui);

void init_keyboard(lv_ui *ui);

extern lv_ui guider_ui;


void setup_scr_screen_App_Select(lv_ui *ui);
void setup_scr_screen_Setting(lv_ui *ui);
void setup_scr_screen_info(lv_ui *ui);

LV_FONT_DECLARE(lv_font_Acme_Regular_26)
LV_FONT_DECLARE(lv_font_montserratMedium_16)
LV_FONT_DECLARE(lv_font_yahei_14)
LV_FONT_DECLARE(lv_font_yahei_15)
LV_FONT_DECLARE(lv_font_montserratMedium_12)
LV_FONT_DECLARE(lv_font_yahei_18)


#ifdef __cplusplus
}
#endif
#endif
