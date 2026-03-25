#include "AWG_Calibrate.h"

#include <inttypes.h>
#include <stdio.h>

#include "lcd_draw_api.h"
#include "UserDefineManage.h"
#include "wave_generator.h"


static void DrawAwgCalibratePage(void);
static char AwgOriginalMsg[10] = {0};                         // 原点校准值
static char AwgAmpMsg[10] = {0};                         // 原点校准值


void AWG_Calibrate_Enter(void) {
    DrawAwgCalibratePage();
    WaveGen_DC(1000, 0, 0, 0);      // 生成0V
}


typedef enum AwgCalPage{
    AwgCal_Origin_Main = 0,
    AwgCal_Origin_Edit,
    AwgCal_Output_Main,
    AwgCal_Output_Edit,
    AwgCal_Page_Num
} AwgCalPage_e;

static AwgCalPage_e AwgCal_Page_Current = AwgCal_Origin_Main;   // 当前的页面


static void AwgCal_Origin_Main_handler(KeyEventMsg_t msg);
static void AwgCal_Origin_Edit_handler(KeyEventMsg_t msg);
static void AwgCal_Output_Main_handler(KeyEventMsg_t msg);
static void AwgCal_Output_Edit_handler(KeyEventMsg_t msg);

static void (*AwgCalPage_handlers[AwgCal_Page_Num])(KeyEventMsg_t) = {
    [AwgCal_Origin_Main] = AwgCal_Origin_Main_handler,
    [AwgCal_Origin_Edit] = AwgCal_Origin_Edit_handler,
    [AwgCal_Output_Main] = AwgCal_Output_Main_handler,
    [AwgCal_Output_Edit]  = AwgCal_Output_Edit_handler
};


void awg_cal_handler(KeyEventMsg_t msg) {
    AwgCalPage_handlers[AwgCal_Page_Current](msg);
}

// 绘制波形发生器校准初始页面
static void DrawAwgCalibratePage(void) {
    // ===== 底色绘制 ===== //
    lcd_draw_rect(0, 0, 319, 31, 0x1908, 1);
    lcd_draw_rect(0, 32, 319, 239, 0x18c6, 1);
    // ===== 顶栏绘制 ===== //
    lcd_draw_line(0, 31, 319-1, 31, 0x11ac);
    lcd_draw_string(105, 6, "信号源校准", &yahei20x20, 0x24be, 0x1908, 3);
    // ===== 进度提示绘制 ===== //
    lcd_draw_round_rect(18,47,108,73,8,0x048A,1);
    lcd_draw_string(28, 52, "原点校准", &yahei16x16, 0xFFFF, 0x048A, 2);
    lcd_draw_round_rect(212,47,302,73,8,0x632C,1);
    lcd_draw_string(222, 52, "输出校准", &yahei16x16, 0xDEFB, 0x632C, 2);
    // ===== 显示区域绘制 ===== //
    lcd_draw_round_rect(15,82,305,146,10,0x0000,1);
    lcd_draw_round_rect(15,82,305,146,10,0x632C,0);
    lcd_draw_string(58,90,"------", &DIN_Medium32x48,0xeb0c,0x0000,2);
    //  ===== 提示框绘制 ===== //
    lcd_draw_round_rect(15,158,305,188,8,0x29a7,1);
    lcd_draw_round_rect(15,158,305,188,8,0x632C,0);
    lcd_draw_string(25,165,"请接万用表，使数值接近0。", &yahei16x16,0xdf3c,0x29a7,1);
    // ====== 调节区绘制 ===== //
    lcd_draw_round_rect(15,200,212,231,8,0x07E0,0);
    lcd_draw_string(25,208,"输出原点:", &yahei16x16,0xFFFF,0x18c6,1);
    snprintf(AwgOriginalMsg, sizeof(AwgOriginalMsg), "%+03" PRId16, UserParam.DDS_Original);
    lcd_draw_string(142,207,AwgOriginalMsg, &yahei18x18,0xFFFF,0x18c6,2);

    // ===== 下一步按钮绘制 ===== //
    lcd_draw_round_rect(220,200,305,231,8,0x632C,1);
    lcd_draw_string(235,207,"下一步", &yahei18x18,0xFFFF,0x632C,1);
}


static uint8_t AwgCal_Origin_Page_current = 0;
static void AwgCal_Origin_Main_handler(KeyEventMsg_t msg) {
        // 如果 下键按下 & 编码器右转
    if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT) {
        AwgCal_Origin_Page_current = (AwgCal_Origin_Page_current < 1) ? (AwgCal_Origin_Page_current + 1) : 0;
    }
    // 如果 上键按下 & 编码器左转
    else if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_LEFT) {
        AwgCal_Origin_Page_current = (AwgCal_Origin_Page_current > 0) ? (AwgCal_Origin_Page_current - 1) : 1;
    }
    // 如果 确认键按下 & 编码器按下
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        if (AwgCal_Origin_Page_current == 0) {                                   // 如果选中的是 参数编辑 框
            AwgCal_Page_Current = AwgCal_Origin_Edit;                       // 将当前页面移交给 原点编辑 页面
            lcd_draw_line(162,225,191,225,0x87d3);                // 绘制编辑页面的下划线示意条

        }
        else if (AwgCal_Origin_Page_current == 1) {                                      // 如果选中的是 下一步 按钮
            AwgCal_Page_Current = AwgCal_Output_Main;                               // 将当前页面移交给 输出校准 主页面
            // 更新页面元素
            lcd_draw_round_rect(15,200,212,231,8,0x07E0,0);
            lcd_draw_round_rect(220,200,305,231,8,0x632C,0);
            lcd_draw_string(25,208,"输出系数:", &yahei16x16,0xFFFF,0x18c6,1);
            snprintf(AwgAmpMsg, sizeof(AwgAmpMsg), "%.3f", UserParam.DDS_Factor);
            lcd_draw_string(110,207,AwgAmpMsg, &yahei18x18,0xFFFF,0x18c6,2);

            lcd_draw_string(25,165,"请接万用表，使数值接近0.15。", &yahei16x16,0xdf3c,0x29a7,1);
            WaveGen_DC(1000, 0, 0.15f, 0);      // 生成1V

            lcd_draw_round_rect(18,47,108,73,8,0x632C,1);
            lcd_draw_string(28, 52, "原点校准", &yahei16x16, 0xDEFB, 0x632C, 2);
            lcd_draw_round_rect(212,47,302,73,8,0x048A,1);
            lcd_draw_string(222, 52, "输出校准", &yahei16x16, 0xFFFF, 0x048A, 2);
            
            StartBeezer(0);
            return;
        }
    }
    else {return;}
    
    StartBeezer(0);
    // 更新选择焦点框
    if (AwgCal_Origin_Page_current == 0) {
        lcd_draw_round_rect(15,200,212,231,8,0x07E0,0);
        lcd_draw_round_rect(220,200,305,231,8,0x632C,0);
    } else {
        lcd_draw_round_rect(15,200,212,231,8,0x632C,0);
        lcd_draw_round_rect(220,200,305,231,8,0x07E0,0);
    }    
}

static void AwgCal_Origin_Edit_handler(KeyEventMsg_t msg) {
    if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_RIGHT) {
        UserParam.DDS_Original += 1;
    }
    else if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_LEFT) {
        UserParam.DDS_Original -= 1;
    }
    else if (msg.key == KEY_SET || msg.key == KEY_ENCODER) {
        AwgCal_Page_Current = AwgCal_Origin_Main;
        lcd_draw_line(162,225,191,225,0x18c6);
    }
    else {return;}
    StartBeezer(0);
    snprintf(AwgOriginalMsg, sizeof(AwgOriginalMsg), "%+03" PRId16, UserParam.DDS_Original);
    lcd_draw_string(142,207,AwgOriginalMsg, &yahei18x18,0xFFFF,0x18c6,2);
    WaveGen_DC(1000, 0, 0, 0);      // 更新信号生成
}

static uint8_t AwgCal_Output_Page_current = 0;

static void AwgCal_Output_Main_handler(KeyEventMsg_t msg) {
            // 如果 下键按下 & 编码器右转
    if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT) {
        AwgCal_Output_Page_current = (AwgCal_Output_Page_current < 1) ? (AwgCal_Output_Page_current + 1) : 0;
    }
    // 如果 上键按下 & 编码器左转
    else if (msg.key == KEY_UP || msg.event == ENCODER_EVENT_LEFT) {
        AwgCal_Output_Page_current = (AwgCal_Output_Page_current > 0) ? (AwgCal_Output_Page_current - 1) : 1;
    }
    // 如果 确认键按下 & 编码器按下
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        if (AwgCal_Output_Page_current == 0) {                                   // 如果选中的是 参数编辑 框
            AwgCal_Page_Current = AwgCal_Output_Edit;                            // 将当前页面移交给 输出编辑 页面
            lcd_draw_line(110,225,194,225,0x87d3);
        }
        else if (AwgCal_Output_Page_current == 1) {                                      // 如果选中的是 下一步 按钮
            UserParam_SaveAllValues();  // 保存配置
            Calibrate_ReturnToMain();   // 返回主界面
            StartBeezer(0);
            return;
        }
    }
    else {return;}

    StartBeezer(0);
    // 更新选择焦点框
    if (AwgCal_Output_Page_current == 0) {
        lcd_draw_round_rect(15,200,212,231,8,0x07E0,0);
        lcd_draw_round_rect(220,200,305,231,8,0x632C,0);
    } else {
        lcd_draw_round_rect(15,200,212,231,8,0x632C,0);
        lcd_draw_round_rect(220,200,305,231,8,0x07E0,0);
    }
}

static void AwgCal_Output_Edit_handler(KeyEventMsg_t msg) {
    if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_RIGHT) {
        UserParam.DDS_Factor += 0.001f;
    }
    else if (msg.key == KEY_ENCODER && msg.event == ENCODER_EVENT_LEFT) {
        UserParam.DDS_Factor -= 0.001f;
    }
    else if (msg.key == KEY_SET || msg.key == KEY_ENCODER) {
        AwgCal_Page_Current = AwgCal_Output_Main;
        lcd_draw_line(110,225,194,225,0x18c6);
    }
    else {return;}
    StartBeezer(0);
    snprintf(AwgAmpMsg, sizeof(AwgAmpMsg), "%.3f", UserParam.DDS_Factor);
    lcd_draw_string(110,207,AwgAmpMsg, &yahei18x18,0xFFFF,0x18c6,2);

    WaveGen_DC(1000, 0, 0.15f, 0);      // 生成1V

}







