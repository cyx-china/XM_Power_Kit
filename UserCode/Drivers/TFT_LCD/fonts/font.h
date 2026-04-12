#ifndef XM_POWER_KIT_FONT_H
#define XM_POWER_KIT_FONT_H

#include <stdint.h>

#define FONT_PATH_MAX      48     // 足够容纳 "0:/fonts/任意合理长度的文件名.bin"

// 字体信息结构体
typedef struct {
    uint8_t         font_width;                      // 字体宽度
    uint8_t         font_height;                     // 字体高度
    uint16_t        font_num;                        // 字符个数
    char            path[FONT_PATH_MAX];             // 完整路径，例如 "0:/fonts/font16x16.bin"
    const uint8_t   (*font_map)[4];                  // 查找表指针
} FontInfo;


/*==================== 字库数据声明 =====================*/

extern const FontInfo DIN_Medium32x50;
extern const FontInfo yahei16x20;
extern const FontInfo JetBrainsMono16x22;
extern const FontInfo ScreenMatrix20x28;
extern const FontInfo JetBrainsMono14x18;
extern const FontInfo KaiTi16x20;
extern const FontInfo YPLXT18x18;
extern const FontInfo yahei18x18;
extern const FontInfo yahei20x20;
extern const FontInfo yahei16x16;
extern const FontInfo DIN_Medium32x48;
extern const FontInfo JetBrainsMono10x14;
extern const FontInfo yahei12x12;
extern const FontInfo MapleMono9x12;
extern const FontInfo yahei24x24;

#endif //XM_POWER_KIT_FONT_H