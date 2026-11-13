/**
  ******************************************************************************
  * @file           : gui_diag.c
  * @brief          : 设备信息页（自研 GUI）：固件/编译时间/UID/外部 Flash/校准状态/传感器/参数/堆
  * @date           : 2026/11/13
  ******************************************************************************
  * 从"关于"页按 下键 / 编码器右转 进入，按 SET 或 MODE 返回。
  * 只用于查看，不改任何参数。
  ******************************************************************************
  */

#include "gui.h"
#include "UserTask.h"
#include "UserDefineManage.h"
#include "i2c.h"
#include "os_handles.h"
#include "Flash.h"
#include "SerialCoreTask.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"

#include <stdio.h>
#include <string.h>

/*=============================== 布局参数 ===============================*/
#define DIAG_HEAD_H        36U
#define DIAG_X_LABEL       12U
#define DIAG_X_VALUE       112U
#define DIAG_ROW_Y0        42U
#define DIAG_ROW_H         22U
#define DIAG_ITEM_NUM      8U

typedef struct {
    const char *label;
    char value[24];
} DiagItem_t;

static DiagItem_t s_items[DIAG_ITEM_NUM];

/*=============================== 取值 ===============================*/
/* 三颗 I2C 器件在线探测：INA226 / TMP102（I2C1）、MCP4725（I2C3） */
static uint8_t diag_probe_i2c(void) {
    uint8_t online = 0;

    osMutexAcquire(IIC1_MutexHandle, osWaitForever);
    if (HAL_I2C_IsDeviceReady(&hi2c1, 0x80, 2, 20) == HAL_OK) {
        online++;
    }
    if (HAL_I2C_IsDeviceReady(&hi2c1, 0x90, 2, 20) == HAL_OK) {
        online++;
    }
    osMutexRelease(IIC1_MutexHandle);

    osMutexAcquire(IIC3_MutexHandle, osWaitForever);
    if (HAL_I2C_IsDeviceReady(&hi2c3, 0xC0, 2, 20) == HAL_OK) {
        online++;
    }
    osMutexRelease(IIC3_MutexHandle);

    return online;
}

/* 四组校准是否被动过（与默认值不同即认为已校准） */
static uint8_t diag_cal_count(void) {
    uint8_t done = 0;

    if (!UserParam_IsDefault(DPS_Voltage_Original)) {
        done++;
    }
    if (!UserParam_IsDefault(OSC_Original)) {
        done++;
    }
    if (!UserParam_IsDefault(DDS_Original)) {
        done++;
    }
    if (!UserParam_IsDefault(DMM_Voltage_Original)) {
        done++;
    }
    return done;
}

static void diag_build(void) {
    uint32_t total_kb = 0, free_kb = 0;
    uint32_t flash_mb = 0;
    uint32_t free_pct = 0;
    uint32_t heap = (uint32_t) xPortGetFreeHeapSize();
    uint8_t cap_code = (uint8_t) ((FLASH_JEDEC_ID >> 16) & 0xFFU);

    s_items[0].label = "固件";
    (void) snprintf(s_items[0].value, sizeof(s_items[0].value), "%s", VERSION_STR);

    s_items[1].label = "编译";
    (void) snprintf(s_items[1].value, sizeof(s_items[1].value), "%s", __DATE__);

    s_items[2].label = "UID";
    (void) snprintf(s_items[2].value, sizeof(s_items[2].value), "%08lX%08lX",
                    (unsigned long) HAL_GetUIDw1(), (unsigned long) HAL_GetUIDw2());

    if (cap_code >= 20U) {
        flash_mb = (1UL << cap_code) >> 20;
    }
    if (FileService_GetCapacity(&total_kb, &free_kb) && (total_kb != 0U)) {
        free_pct = (free_kb * 100U) / total_kb;
    }
    s_items[3].label = "存储";
    (void) snprintf(s_items[3].value, sizeof(s_items[3].value), "%luMB %lu%%",
                    (unsigned long) flash_mb, (unsigned long) free_pct);

    s_items[4].label = "校准";
    (void) snprintf(s_items[4].value, sizeof(s_items[4].value), "%u/4 SET", (unsigned) diag_cal_count());

    s_items[5].label = "传感器";
    (void) snprintf(s_items[5].value, sizeof(s_items[5].value), "%u/3", (unsigned) diag_probe_i2c());

    s_items[6].label = "参数";
    (void) snprintf(s_items[6].value, sizeof(s_items[6].value), "%u/%uB",
                    (unsigned) Param_Number, (unsigned) PARAM_TOTAL_BYTES);

    s_items[7].label = "堆";
    (void) snprintf(s_items[7].value, sizeof(s_items[7].value), "%lu.%luKB",
                    (unsigned long) (heap / 1024U), (unsigned long) (((heap % 1024U) * 10U) / 1024U));
}

/*=============================== 绘制 ===============================*/
static void diag_draw(void) {
    uint8_t i;

    Gui_Clear(GUI_COL_BG);

    Gui_TextCenter(160, 8, "设 备 信 息", &ui_text, GUI_COL_TITLE, GUI_COL_BG);
    Gui_Line(0, DIAG_HEAD_H - 1U, 319, DIAG_HEAD_H - 1U, GUI_COL_CARD);

    for (i = 0; i < DIAG_ITEM_NUM; i++) {
        uint16_t y = (uint16_t) (DIAG_ROW_Y0 + (uint16_t) i * DIAG_ROW_H);

        Gui_Text(DIAG_X_LABEL, y, s_items[i].label, &ui_text, GUI_COL_TEXT_DIM, GUI_COL_BG);
        Gui_Text(DIAG_X_VALUE, y, s_items[i].value, &ui_mono, GUI_COL_TEXT, GUI_COL_BG);
    }

    Gui_Line(20, 216, 299, 216, GUI_COL_CARD);
    Gui_TextCenter(160, 220, "SET / MODE 返回", &ui_text, GUI_COL_TEXT_DIM, GUI_COL_BG);
}

/*=============================== 交互 ===============================*/
static void diag_on_key(GuiKey_t key) {
    switch (key) {
        case GUI_KEY_ENTER:
        case GUI_KEY_BACK:
        case GUI_KEY_UP:
        case GUI_KEY_LEFT:
            Gui_Back();
            break;
        default:
            break;
    }
}

static void diag_on_enter(void) {
    diag_build();
    diag_draw();
}

const GuiScreen gui_screen_diag = {
    .on_enter = diag_on_enter,
    .on_exit  = NULL,
    .on_key   = diag_on_key,
    .on_tick  = NULL,
    .on_draw  = diag_draw
};
