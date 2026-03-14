/**
 * @file lv_conf.h
 * 精简版配置 - 只保留 btn / label / img / container + 圆角 + default 主题 + FATFS
 * LV_MEM_SIZE 和 LV_MEM_ADR 保持原样
 * LV_DRAW_COMPLEX 必须开启
 */

#if 1 /* Set it to "1" to enable content */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*==================== COLOR SETTINGS ====================*/
#define LV_COLOR_DEPTH     16
#define LV_COLOR_16_SWAP   0
#define LV_COLOR_SCREEN_TRANSP 0
#define LV_COLOR_CHROMA_KEY lv_color_hex(0x00ff00)

/*========================= MEMORY SETTINGS =========================*/
#define LV_MEM_CUSTOM      0
#if LV_MEM_CUSTOM == 0
    #define LV_MEM_SIZE    (32U * 1024U)
    #define LV_MEM_ADR     0x10000000
#endif

#define LV_MEM_BUF_MAX_NUM  8
#define LV_MEMCPY_MEMSET_STD 0

/*==================== HAL SETTINGS ====================*/
#define LV_DISP_DEF_REFR_PERIOD 30
#define LV_INDEV_DEF_READ_PERIOD 30
#define LV_TICK_CUSTOM      0
#define LV_DPI_DEF          130

/*======================= FEATURE CONFIGURATION =======================*/

/* Drawing - 必须开启以支持圆角 */
#define LV_DRAW_COMPLEX     1
#if LV_DRAW_COMPLEX != 0
    #define LV_SHADOW_CACHE_SIZE 0
    #define LV_CIRCLE_CACHE_SIZE 4
#endif

#define LV_LAYER_SIMPLE_BUF_SIZE          (4 * 1024)
#define LV_LAYER_SIMPLE_FALLBACK_BUF_SIZE (4 * 1024)
#define LV_IMG_CACHE_DEF_SIZE 0

/* GPU - 全关闭（除非你有硬件加速） */
#define LV_USE_GPU_ARM2D    0
#define LV_USE_GPU_STM32_DMA2D 0
#define LV_USE_GPU_NXP_PXP  0
#define LV_USE_GPU_NXP_VG_LITE 0

/* Logging & Assert */
#define LV_USE_LOG          0
#define LV_USE_ASSERT_NULL  1
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_ASSERT_STYLE 0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ   0

/* Others */
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR  0
#define LV_USE_REFR_DEBUG   0
#define LV_SPRINTF_CUSTOM   0
#define LV_USE_USER_DATA    1

/*================== FONT USAGE ==================*/
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_DEFAULT    &lv_font_montserrat_16

#define LV_FONT_FMT_TXT_LARGE   0
#define LV_USE_FONT_COMPRESSED  0
#define LV_USE_FONT_SUBPX       0
#define LV_USE_FONT_PLACEHOLDER 1

/*================= TEXT SETTINGS =================*/
#define LV_TXT_ENC          LV_TXT_ENC_UTF8
#define LV_USE_BIDI         0
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*================== WIDGET USAGE =================*/
#define LV_USE_ARC          0
#define LV_USE_BAR          1
#define LV_USE_BTN          1
#define LV_USE_BTNMATRIX    0
#define LV_USE_CANVAS       0
#define LV_USE_CHECKBOX     0
#define LV_USE_DROPDOWN     1
#define LV_USE_IMG          1
#define LV_USE_LABEL        1
#define LV_USE_LINE         0
#define LV_USE_ROLLER       0
#define LV_USE_SLIDER       1
#define LV_USE_SWITCH       1
#define LV_USE_TEXTAREA     0
#define LV_USE_TABLE        0

/*================== EXTRA COMPONENTS ==================*/
#define LV_USE_ANIMIMG      0
#define LV_USE_CALENDAR     0
#define LV_USE_CHART        0
#define LV_USE_COLORWHEEL   0
#define LV_USE_IMGBTN       0
#define LV_USE_KEYBOARD     0
#define LV_USE_LED          0
#define LV_USE_LIST         0
#define LV_USE_MENU         0
#define LV_USE_METER        0
#define LV_USE_MSGBOX       0
#define LV_USE_SPAN         0
#define LV_USE_SPINBOX      0
#define LV_USE_SPINNER      0
#define LV_USE_TABVIEW      0
#define LV_USE_TILEVIEW     0
#define LV_USE_WIN          0
#define LV_USE_SVG          1

/* Layouts */
#define LV_USE_FLEX         1
#define LV_USE_GRID         0

/* Themes - 你在使用 default */
#define LV_USE_THEME_DEFAULT 1
#if LV_USE_THEME_DEFAULT
    #define LV_THEME_DEFAULT_DARK 0
    #define LV_THEME_DEFAULT_GROW 1
    #define LV_THEME_DEFAULT_TRANSITION_TIME 80
#endif
#define LV_USE_THEME_BASIC   0
#define LV_USE_THEME_MONO    0

/* File system - 保留 FATFS */
#define LV_USE_FS_FATFS                 1
#if LV_USE_FS_FATFS
    #define LV_FS_FATFS_LETTER          'S'
    #define LV_FS_FATFS_PATH            "0:"
    #define LV_FS_FATFS_CACHE_SIZE      0
#endif

/* Image decoders  */
#define LV_USE_PNG          0
#define LV_USE_BMP          0
#define LV_USE_SJPG         0
#define LV_USE_GIF          0
#define LV_USE_QRCODE       0
#define LV_USE_FREETYPE     0
#define LV_USE_RLOTTIE      0
#define LV_USE_FFMPEG       0

/* Others */
#define LV_USE_SNAPSHOT     0
#define LV_USE_MONKEY       0
#define LV_USE_GRIDNAV      0
#define LV_USE_FRAGMENT     0
#define LV_USE_IMGFONT      0
#define LV_USE_MSG          0
#define LV_USE_IME_PINYIN   0

/*================== EXAMPLES & DEMO ==================*/
#define LV_BUILD_EXAMPLES                   0
#define LV_USE_DEMO_WIDGETS                 0
#define LV_USE_DEMO_KEYPAD_AND_ENCODER      0
#define LV_USE_DEMO_BENCHMARK               0
#define LV_USE_DEMO_STRESS                  0
#define LV_USE_DEMO_MUSIC                   0

#endif /*LV_CONF_H*/

#endif /*End of content enable*/