#include "CalCommon.h"

#include "lcd_draw_api.h"       // lcd_draw_* + font.h(yahei16x16) + cmsis_os2.h
#include "UserDefineManage.h"   // UserParam_SaveAllValues

void Cal_ShowMessage(const char *text, uint16_t ft_color) {
    lcd_draw_round_rect(15, 158, 305, 188, 8, 0x29a7, 1);
    lcd_draw_round_rect(15, 158, 305, 188, 8, 0x632C, 0);
    lcd_draw_string(25, 165, text, &yahei16x16, ft_color, 0x29a7, 1);
}

bool Cal_SaveParamsChecked(void) {
    if (UserParam_SaveAllValues() == HAL_OK) {
        return true;
    }

    Cal_ShowMessage("参数保存失败！", 0xF800);
    StartBeezer(0);
    osDelay(1500);
    return false;
}
