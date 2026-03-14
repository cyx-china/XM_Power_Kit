/**
* @file lv_svg.h
 *
 */

#ifndef LV_PNG_H
#define LV_PNG_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
*********************/
#include <stdint.h>
#include "lv_conf.h"
#if LV_USE_SVG

/*********************
 *      DEFINES
*********************/
#define LV_SVG_KEY 0x54b058c084142178ULL
#define LV_SBG_CODE "01"
#define LV_SVG_PATH "0:/SVG.bin"

/**********************
 *      TYPEDEFS
**********************/

/**********************
 * GLOBAL PROTOTYPES
**********************/

void lv_little_svg_init(void);

/**********************
 *      MACROS
**********************/

#endif /*LV_USE_PNG*/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_PNG_H*/