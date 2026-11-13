#ifndef XM_POWER_KIT_IMAGES_H
#define XM_POWER_KIT_IMAGES_H

#include <stdint.h>


#define IMAGE_PATH_MAX      48     // 足够容纳 "0:/images/任意合理长度的文件名.bin"

// 图片信息结构体，
typedef struct {
    uint16_t        width;
    uint16_t        height;
    char            path[IMAGE_PATH_MAX];     // 完整路径，例如 "0:/images/bootlogo_320x240.bin"
} ImageInfo;


/*==================== 图像数据声明 =====================*/

extern const ImageInfo img_IN_16x20;
extern const ImageInfo img_FIRE_16x20;
extern const ImageInfo img_FAN_16x16;
extern const ImageInfo img_energy_16x20;
extern const ImageInfo img_time_18x18;
extern const ImageInfo img_wave1_212x57;
extern const ImageInfo img_wave2_212x57;
extern const ImageInfo img_wave3_212x57;
extern const ImageInfo img_wave4_212x57;
extern const ImageInfo img_wave5_212x57;
extern const ImageInfo img_wave6_212x57;
extern const ImageInfo img_wave7_212x57;
extern const ImageInfo img_wave8_212x57;
extern const ImageInfo img_wave9_212x57;
extern const ImageInfo img_wave10_212x57;
extern const ImageInfo img_wave11_212x57;
extern const ImageInfo img_wave12_212x57;
extern const ImageInfo img_wave13_212x57;
extern const ImageInfo img_wave14_212x57;
extern const ImageInfo img_DMM_DC_30x30;

/*==================== 主界面 APP 图标（56x56，图片与字体/图标 转换而来） ====================*/
extern const ImageInfo img_dy_56x56;        // 电源
extern const ImageInfo img_sbq_56x56;       // 示波器
extern const ImageInfo img_bxfsq_56x56;     // 信号源
extern const ImageInfo img_wyb_56x56;       // 万用表
extern const ImageInfo img_cgq_56x56;       // 传感器
extern const ImageInfo img_jiaozhun_56x56;  // 校准
extern const ImageInfo img_sz_56x56;        // 设置
extern const ImageInfo img_xinxi_56x56;     // 关于




#endif //XM_POWER_KIT_IMAGES_H