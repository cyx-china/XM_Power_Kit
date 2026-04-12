/**
* @file lv_svg.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_svg.h"
#include <stdbool.h>
#include <stdio.h>

#include "lcd_draw_api.h"
#include "stm32f4xx_hal.h"
#include "os_handles.h"
#include "UserTask.h"
#if LV_USE_SVG

/*********************
*      DEFINES
*********************/


/**********************
*      TYPEDEFS
**********************/
typedef enum {
    PAGE_MAIN = 0,
    PAGE_DIGIT,
    PAGE_NUM
} Page_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/

static uint32_t lv_svg_calculate_regcode(uint64_t dev_num);
static void lv_svg_get_devcode(char code_str[13]);
static HAL_StatusTypeDef lv_svg_save_regcode(uint32_t code);
static uint32_t lv_svg_read_regcode(void);
static void lv_main_handler(KeyEventMsg_t msg);
static void lv_digit_handler(KeyEventMsg_t msg);
static void lv_comfirm_handler(void);
static void lv_svg_draw_page(void);
/**********************
 *  STATIC VARIABLES
 **********************/
static KeyEventMsg_t Keymsg;
static void (*page_handlers[PAGE_NUM])(KeyEventMsg_t) = {
    [PAGE_MAIN] = lv_main_handler,
    [PAGE_DIGIT] = lv_digit_handler,
};
static Page_t current_page = PAGE_MAIN;
static bool init_status = false;
static char code_str[13] = {0};
static uint64_t dev_num = 0;
static uint32_t reg_code = 8888;
/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_little_svg_init(void) {
    init_status = true;
    lv_svg_get_devcode(code_str);
    uint32_t saved_code = lv_svg_read_regcode();
    for (int i = 0; i < 12; i++)
        dev_num = dev_num * 10 + (code_str[i] - '0');
    if (saved_code != 0xFFFF) {
        if (lv_svg_calculate_regcode(dev_num) == saved_code)
            init_status = true;
    }
    if (init_status == false) {
        lv_svg_draw_page();
        Resume_IndevDetectTask();
        for (;;) {
            if (osMessageQueueGet(KeyEventQueueHandle, &Keymsg, NULL, 0) == osOK)
                page_handlers[current_page](Keymsg);

            if (init_status == true) break;
            osDelay(10);
        }
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static uint32_t lv_svg_calculate_regcode(uint64_t dev_num) {
  uint64_t temp = dev_num * LV_SVG_KEY;
  uint32_t code = (uint32_t)(temp >> 32) ^ (uint32_t)temp;
  return code % 1000000;
}

static void lv_svg_get_devcode(char code_str[13]) {
  uint32_t uid0 = HAL_GetUIDw0();
  uint32_t uid1 = HAL_GetUIDw1();
  uint32_t uid2 = HAL_GetUIDw2();
  uint32_t uid_compress = uid0 ^ uid1 ^ uid2;
  sprintf(code_str, "%010lu%s", uid_compress, LV_SBG_CODE);
}

static HAL_StatusTypeDef lv_svg_save_regcode(uint32_t code) {
  res = f_open(&fil, LV_SVG_PATH, FA_READ);
  if (res != FR_OK) {
      f_close(&fil);
      res = f_open(&fil, LV_SVG_PATH, FA_CREATE_ALWAYS | FA_WRITE);
      if (res!= FR_OK) {return HAL_ERROR;}
  }
    UINT bw = 0;
    res = f_write(&fil, &code, 4, &bw);
    f_close(&fil);
    if (res != FR_OK || bw != 4) {
        return HAL_ERROR;
    }
    return HAL_OK;
}

static uint32_t lv_svg_read_regcode(void) {
    uint32_t code = 0;
    res = f_open(&fil, LV_SVG_PATH, FA_READ);
    if (res != FR_OK) {
        f_close(&fil);
        return 0xFFFF;
    }
      res = f_read(&fil, &code, 4, &br);
      f_close(&fil);
      if (res != FR_OK || br != 4) {
          return 0xFFFF;
      }
      return code;
  }

uint8_t main_select = 0;
static void lv_main_handler(KeyEventMsg_t msg) {
    if (msg.key == KEY_DOWN || msg.event == ENCODER_EVENT_RIGHT || msg.event == ENCODER_EVENT_PRESS_RIGHT
        || msg.key == KEY_UP || msg.event == ENCODER_EVENT_LEFT || msg.event == ENCODER_EVENT_PRESS_LEFT) {
        main_select = (main_select + 1) % 2;
    }
    else if ((msg.key == KEY_SET || msg.key == KEY_ENCODER) && msg.event == KEY_EVENT_CLICK) {
        if (main_select == 0) {
            current_page = PAGE_DIGIT;
        } else if (main_select == 1) {
            lv_comfirm_handler();
        }
        StartBeezer(0);
    }
    else{return;}

    StartBeezer(0);
    if (main_select == 0) {
        lcd_draw_round_rect(70,122,249,161,8,0x87d3,0);
        lcd_draw_round_rect(100,169,219,204,8,0x21aa,0);
    }
    else if (main_select == 1) {
        lcd_draw_round_rect(70,122,249,161,8,0x21aa,0);
        lcd_draw_round_rect(100,169,219,204,8,0x87d3,0);
    }
}

static void lv_digit_handler(KeyEventMsg_t msg) {

}

static void lv_comfirm_handler(void) {
    if (lv_svg_calculate_regcode(dev_num) == reg_code) {
        // lv_svg_save_regcode(reg_code);
        init_status = true;
        lcd_draw_rect(59, 99, 259, 139, 0x07e0, 1);
        lcd_draw_string(110,104,"1234",&ScreenMatrix20x28, 0xef7d, 0x07e0, 0);
        osDelay(1500);
        Suspend_IndevDetectTask();
    }
    else {
        lcd_draw_rect(59, 99, 259, 139, 0xf800, 1);
        lcd_draw_string(110,104,"7890",&ScreenMatrix20x28, 0xef7d, 0xf800, 0);
        osDelay(1500);

        current_page = PAGE_MAIN;
        init_status = false;
        lv_svg_draw_page();
    }

}

static void lv_svg_draw_page(void) {
    lcd_draw_rect(0, 0, 319, 31, 0x1908, 1);
    lcd_draw_line(0, 31, 319, 31, 0x11ac);
    lcd_draw_string(106,6,"波形发生器",&yahei20x20, 0x24be, 0x1908, 3);

    lcd_draw_rect(0, 32, 319, 239, 0x18c6, 1);

    lcd_draw_string(130,39,"正弦波",&yahei16x16, 0x24be, 0x18c6, 3);
    lcd_draw_round_rect(50,58,269,97,8,0x1908,1);
    lcd_draw_round_rect(50,58,269,97,8,0x21aa,0);
    lcd_draw_string(67,68,code_str,&KaiTi16x20, 0x24be, 0x1908, 0);

    lcd_draw_string(130,103,"三角波",&yahei16x16, 0x24be, 0x18c6, 3);
    lcd_draw_round_rect(70,122,249,161,8,0x1908,1);
    lcd_draw_round_rect(70,122,249,161,8,0x87d3,0);
    lcd_draw_string(103,132,"000000",&KaiTi16x20, 0x24be, 0x1908, 4);

    lcd_draw_round_rect(100,169,219,204,8,0x1908,1);
    lcd_draw_round_rect(100,169,219,204,8,0x21aa,0);
    lcd_draw_string(135,176,"发生",&yahei20x20, 0xef7d, 0x1908, 10);
}









#endif /*LV_USE_PNG*/