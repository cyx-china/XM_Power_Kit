#ifndef XM_POWER_KIT_CB_PROCESS_H
#define XM_POWER_KIT_CB_PROCESS_H
#include <stdint.h>


void btn_sensor_process(void);
void btn_power_process(void);
void btn_dso_process(void);
void btn_awg_process(void);
void btn_dmm_process(void);
void btn_calibration_process(void);

void Setting_screen_bright_process(void);

void Setting_screen_direct_process(int status);

void Setting_beezer_volume_process(void);

void Setting_beezer_time_process(void);

void Setting_fan_temperature_process(void);

void Setting_screen_sleeptime_process(uint16_t id);

void Setting_fan_on_process(int status);

void Load_Setting_params(void);

void Load_About_Values(void);

void Save_Setting_Params(void);


#endif //XM_POWER_KIT_CB_PROCESS_H