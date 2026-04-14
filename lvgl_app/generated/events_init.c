/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "events_init.h"
#include <stdio.h>

#include "cb_process.h"
#include "lvgl.h"

#if LV_USE_GUIDER_SIMULATOR && LV_USE_FREEMASTER
#include "freemaster_client.h"
#endif


static void screen_App_Select_btn_Power_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        btn_power_process();
        break;
    }
    default:
        break;
    }
}

static void screen_App_Select_btn_DSO_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        btn_dso_process();
        break;
    }
    default:
        break;
    }
}

static void screen_App_Select_btn_DDS_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        btn_awg_process();
        break;
    }
    default:
        break;
    }
}

static void screen_App_Select_btn_DMM_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        btn_dmm_process();
        break;
    }
    default:
        break;
    }
}

static void screen_App_Select_btn_SENSOR_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        btn_user_process();
        break;
    }
    default:
        break;
    }
}

static void screen_App_Select_btn_CLBAT_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        btn_calibration_process();
        break;
    }
    default:
        break;
    }
}

static void screen_App_Select_btn_SET_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        ui_load_scr_animation(&guider_ui, &guider_ui.screen_Setting, guider_ui.screen_Setting_del, &guider_ui.screen_App_Select_del, setup_scr_screen_Setting, LV_SCR_LOAD_ANIM_FADE_ON, 200, 200, true, true);
        Load_Setting_params();
        break;
    }
    default:
        break;
    }
}

static void screen_App_Select_btn_ABOUT_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        ui_load_scr_animation(&guider_ui, &guider_ui.screen_info, guider_ui.screen_info_del, &guider_ui.screen_App_Select_del, setup_scr_screen_info, LV_SCR_LOAD_ANIM_FADE_ON, 200, 200, true, true);
        Load_About_Values();
        break;
    }
    default:
        break;
    }
}

void events_init_screen_App_Select (lv_ui *ui)
{
    lv_obj_add_event_cb(ui->screen_App_Select_btn_Power, screen_App_Select_btn_Power_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_App_Select_btn_DSO, screen_App_Select_btn_DSO_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_App_Select_btn_DDS, screen_App_Select_btn_DDS_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_App_Select_btn_DMM, screen_App_Select_btn_DMM_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_App_Select_btn_SENSOR, screen_App_Select_btn_SENSOR_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_App_Select_btn_CLBAT, screen_App_Select_btn_CLBAT_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_App_Select_btn_SET, screen_App_Select_btn_SET_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_App_Select_btn_ABOUT, screen_App_Select_btn_ABOUT_event_handler, LV_EVENT_ALL, ui);
}

static void screen_Setting_ddlist_SleepTime_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_VALUE_CHANGED:
    {
        uint16_t id = lv_dropdown_get_selected(guider_ui.screen_Setting_ddlist_SleepTime);
        Setting_screen_sleeptime_process(id);
        break;
    }
    default:
        break;
    }
}

static void screen_Setting_slider_ScrenLight_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_VALUE_CHANGED:
    {
        Setting_screen_bright_process();
        break;
    }
    default:
        break;
    }
}

static void screen_Setting_sw_ShowFlip_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_VALUE_CHANGED:
    {
        lv_obj_t * status_obj = lv_event_get_target(e);
        int status = lv_obj_has_state(status_obj, LV_STATE_CHECKED) ? true : false;
        Setting_screen_direct_process(status);
        break;
    }
    default:
        break;
    }
}

static void screen_Setting_slider_BeezerVolume_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_VALUE_CHANGED:
    {
        Setting_beezer_volume_process();
        break;
    }
    default:
        break;
    }
}

static void screen_Setting_slider_BeezerTime_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_VALUE_CHANGED:
    {
        Setting_beezer_time_process();
        break;
    }
    default:
        break;
    }
}

static void screen_Setting_sw_FanEnable_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_VALUE_CHANGED:
    {
        lv_obj_t * status_obj = lv_event_get_target(e);
        int status = lv_obj_has_state(status_obj, LV_STATE_CHECKED) ? true : false;
        Setting_fan_on_process(status);

        break;
    }
    default:
        break;
    }
}

static void screen_Setting_slider_FanStartTemp_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_VALUE_CHANGED:
    {
        Setting_fan_temperature_process();
        break;
    }
    default:
        break;
    }
}

static void screen_Setting_btn_back_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        Save_Setting_Params();
        ui_load_scr_animation(&guider_ui, &guider_ui.screen_App_Select, guider_ui.screen_App_Select_del, &guider_ui.screen_Setting_del, setup_scr_screen_App_Select, LV_SCR_LOAD_ANIM_FADE_ON, 200, 200, true, true);
        break;
    }
    default:
        break;
    }
}

void events_init_screen_Setting (lv_ui *ui)
{
    lv_obj_add_event_cb(ui->screen_Setting_ddlist_SleepTime, screen_Setting_ddlist_SleepTime_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_Setting_slider_ScrenLight, screen_Setting_slider_ScrenLight_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_Setting_sw_ShowFlip, screen_Setting_sw_ShowFlip_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_Setting_slider_BeezerVolume, screen_Setting_slider_BeezerVolume_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_Setting_slider_BeezerTime, screen_Setting_slider_BeezerTime_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_Setting_sw_FanEnable, screen_Setting_sw_FanEnable_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_Setting_slider_FanStartTemp, screen_Setting_slider_FanStartTemp_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_Setting_btn_back, screen_Setting_btn_back_event_handler, LV_EVENT_ALL, ui);
}

static void screen_info_btn_1_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        ui_load_scr_animation(&guider_ui, &guider_ui.screen_App_Select, guider_ui.screen_App_Select_del, &guider_ui.screen_info_del, setup_scr_screen_App_Select, LV_SCR_LOAD_ANIM_FADE_ON, 200, 200, true, true);
        break;
    }
    default:
        break;
    }
}

void events_init_screen_info (lv_ui *ui)
{
    lv_obj_add_event_cb(ui->screen_info_btn_1, screen_info_btn_1_event_handler, LV_EVENT_ALL, ui);
}


void events_init(lv_ui *ui)
{

}
