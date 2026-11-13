#ifndef XM_POWER_KIT_CAL_COMMON_H
#define XM_POWER_KIT_CAL_COMMON_H

#include <stdbool.h>
#include <stdint.h>

// 校准页面共用的提示框（y=158..188，与各模块原来的提示位置一致）
void Cal_ShowMessage(const char *text, uint16_t ft_color);

// 保存全部用户参数到外部 Flash。
// 成功返回 true；失败时显示红色提示 + 蜂鸣并停留 1.5 秒，再返回 false
// （就算保存失败调用方也照常返回上级页面，避免卡死在校准界面）。
bool Cal_SaveParamsChecked(void);

#endif
