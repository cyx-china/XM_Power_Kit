#include "cb_process.h"
#include "UserTask.h"
#include "os_handles.h"
#include "cmsis_os2.h"
#include "gui_guider.h"
#include "lv_slider.h"
#include "ST7789.h"
#include "tim.h"
#include "usbd_cdc_if.h"
#include "UserDefineManage.h"

/*============================================
 * APP选择界面回调函数
 ============================================*/

// 数控电源按钮回调函数
void btn_power_process(void) {
    AppListType APP = APP_POWER;
    osMessageQueuePut(AppSwitchQueueHandle,&APP,0,10);
}

// 示波器按钮回调函数
void btn_dso_process(void) {
    AppListType APP = APP_DSO;
    osMessageQueuePut(AppSwitchQueueHandle,&APP,0,10);
}

// 波形发生器按钮回调函数
void btn_awg_process(void) {
    AppListType APP = APP_AWG;
    osMessageQueuePut(AppSwitchQueueHandle,&APP,0,10);
}

// 万用表按钮回调函数
void btn_dmm_process(void) {
    AppListType APP = APP_DMM;
    osMessageQueuePut(AppSwitchQueueHandle,&APP,0,10);
}

// 用户APP按钮回调函数
void btn_user_process(void) {
#if USE_USER_APP
    AppListType APP = APP_USER;
    osMessageQueuePut(AppSwitchQueueHandle,&APP,0,10);
#endif
}

// 校准按钮回调函数
void btn_calibration_process(void) {
    AppListType APP = APP_CAL;
    osMessageQueuePut(AppSwitchQueueHandle,&APP,0,10);
}

/*============================================
 * 设置界面函数
 ============================================*/
bool IsValueChanged = false;

static const uint16_t g_SleepTimeMap[6] = {0, 60, 300, 600, 1800, 3600};
// 屏幕休眠时间下拉框回调
void Setting_screen_sleeptime_process(uint16_t id)
{
    IsSleeping = false;
    SleepCounter = g_SleepTimeMap[id];

    UserParam.Screen_Sleeptime = g_SleepTimeMap[id];
    IsValueChanged = true;
}

// 屏幕亮度滑动条回调
void Setting_screen_bright_process(void) {
    // 更新lable标签
    int32_t slider_val = lv_slider_get_value(guider_ui.screen_Setting_slider_ScrenLight);
    char buf[8];
    lv_snprintf(buf, sizeof(buf), "%ld%%", slider_val); // slider_val范围是10 ~ 100
    lv_label_set_text(guider_ui.screen_Setting_label_var, buf);

    // 更新屏幕亮度
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 4999 * slider_val / 100);

    // 更新UserParam
    UserParam.Screen_Brightness = slider_val;
    IsValueChanged = true;
}

// 屏幕显示方向回调
void Setting_screen_direct_process(int status) {
    // if (status == 0) {
    //     LCD_WR_CMD(0x36);        // 显示方向设置（MY/MX/MV/ML控制）
    //     LCD_WR_DATA(0x60);                // 正向
    // } else {
    //     LCD_WR_CMD(0x36);        // 显示方向设置（MY/MX/MV/ML控制）
    //     LCD_WR_DATA(0xA0);                // 反向
    // }

    if (status == 0) {
        LCD_WR_CMD(0x21);        // 开启颜色翻转
    } else {
        LCD_WR_CMD(0x20);        // 关闭颜色翻转
    }

    //lv_obj_invalidate(lv_scr_act());      // 标记全屏为脏，刷新全屏

    // 更新UserParam
    UserParam.Screen_ShowFlip = status;
    IsValueChanged = true;
}


// 蜂鸣器音量滑动条回调
void Setting_beezer_volume_process(void) {
    int32_t slider_val = lv_slider_get_value(guider_ui.screen_Setting_slider_BeezerVolume);
    char buf[8];
    lv_snprintf(buf, sizeof(buf), "%ld%%", slider_val);
    lv_label_set_text(guider_ui.screen_Setting_label_2, buf);

    __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1, (slider_val - 1) / 2);

    // 更新UserParam
    UserParam.Beezer_Volume = slider_val;
    IsValueChanged = true;
}

// 蜂鸣器蜂鸣时间滑动条回调
void Setting_beezer_time_process(void) {
    int32_t slider_val = lv_slider_get_value(guider_ui.screen_Setting_slider_BeezerTime);
    char buf[8];
    lv_snprintf(buf, sizeof(buf), "%ldms", slider_val * 10);
    lv_label_set_text(guider_ui.screen_Setting_label_4, buf);

    // 更新UserParam
    UserParam.Beezer_Time = slider_val;
    IsValueChanged = true;
}

// 风扇启转温度滑动条回调
void Setting_fan_temperature_process(void) {
    int32_t slider_val = lv_slider_get_value(guider_ui.screen_Setting_slider_FanStartTemp);
    char buf[8];
    lv_snprintf(buf, sizeof(buf), "%ld℃", slider_val);
    lv_label_set_text(guider_ui.screen_Setting_label_6, buf);

    // 更新UserParam
    UserParam.Fan_StartTemperture = slider_val;
    IsValueChanged = true;
}

// 风扇使能开关回调
void Setting_fan_on_process(int status) {
    // 更新UserParam
    UserParam.Fan_Enable = status;
    IsValueChanged = true;
}

static inline uint8_t get_sleep_time_index_fast(uint16_t val) {
    switch(val) {
        case 0:     return 0;
        case 60:    return 1;
        case 300:   return 2;
        case 600:   return 3;
        case 1800:  return 4;
        case 3600:  return 5;
        default:    return 0;
    }
}

// 初始化函数
void inline Load_Setting_params() {
    // 更新屏幕亮度下拉框
    uint16_t index = get_sleep_time_index_fast(UserParam.Screen_Sleeptime);
    lv_dropdown_set_selected(guider_ui.screen_Setting_ddlist_SleepTime, index);
    // 更新亮度条与对于标签
    lv_slider_set_value(guider_ui.screen_Setting_slider_ScrenLight, UserParam.Screen_Brightness, LV_ANIM_OFF);
    char buf1[8],buf2[8],buf3[8],buf4[8];
    lv_snprintf(buf1, sizeof(buf1), "%d%%", UserParam.Screen_Brightness);
    lv_label_set_text(guider_ui.screen_Setting_label_var, buf1);
    // 更新显示翻转switch
    lv_obj_add_state(guider_ui.screen_Setting_sw_ShowFlip, UserParam.Screen_ShowFlip ? LV_STATE_CHECKED : 0);
    // 更新蜂鸣器音量条与标签
    lv_slider_set_value(guider_ui.screen_Setting_slider_BeezerVolume, UserParam.Beezer_Volume, LV_ANIM_OFF);
    lv_snprintf(buf2, sizeof(buf2), "%d%%", UserParam.Beezer_Volume);
    lv_label_set_text(guider_ui.screen_Setting_label_2, buf2);
    // 更新蜂鸣器时常进度条与标签
    lv_slider_set_value(guider_ui.screen_Setting_slider_BeezerTime, UserParam.Beezer_Time, LV_ANIM_OFF);
    lv_snprintf(buf3, sizeof(buf3), "%dms", UserParam.Beezer_Time * 10);
    lv_label_set_text(guider_ui.screen_Setting_label_4, buf3);
    //更新风扇使能switch
    lv_obj_add_state(guider_ui.screen_Setting_sw_FanEnable, UserParam.Fan_Enable ? LV_STATE_CHECKED : 0);
    //更新风扇启转温度条与标签
    lv_slider_set_value(guider_ui.screen_Setting_slider_FanStartTemp, UserParam.Fan_StartTemperture, LV_ANIM_OFF);
    lv_snprintf(buf4, sizeof(buf4), "%d℃", UserParam.Fan_StartTemperture);
    lv_label_set_text(guider_ui.screen_Setting_label_6, buf4);
}

void inline Load_About_Values() {
    lv_label_set_text(guider_ui.screen_info_label_Version, Sys_Version);
}

void inline Save_Setting_Params() {
    if (IsValueChanged) {
        UserParam_SaveAllValues();      // 保存设置
    }

}

