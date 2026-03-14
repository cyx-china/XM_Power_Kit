#include "lcd_draw_api.h"
#include "freertos_os2.h"

#define COORD(x, y)  (((uint32_t)x << 16) | (uint16_t)y)

static osStatus_t lcd_draw_send_cmd(LcdFlushType flush_type,        // 绘制图案的类型
                                    uint32_t start_point,           // 起始点（x,y已结合）
                                    uint32_t end_point,             // 终点
                                    uint16_t color,                 // 颜色（前景色）
                                    uint8_t is_filled,              // 是否填充
                                    uint16_t param1,                // 参数1
                                    uint16_t param2,                // 参数2
                                    const ImageInfo* image,         // 图片的句柄
                                    const FontInfo* font,           // 字库的句柄
                                    const char* str)
{
    // 分配refresh_msg_t内存
    refresh_msg_t* msg = (refresh_msg_t*)pvPortMalloc(sizeof(refresh_msg_t));
    if (msg == NULL) {
        return osErrorNoMemory;
    }

    // 分配GFX_DrawCommand_t内存
    GFX_DrawCommand_t* cmd = (GFX_DrawCommand_t*)pvPortMalloc(sizeof(GFX_DrawCommand_t));
    if (cmd == NULL) {
        vPortFree(msg);
        return osErrorNoMemory;
    }

    // 初始化绘图命令
    memset(cmd, 0, sizeof(GFX_DrawCommand_t));
    cmd->flush_type = flush_type;
    cmd->start_point = start_point;
    cmd->end_point = end_point;
    cmd->color = color;
    cmd->is_filled = is_filled;
    cmd->param1 = param1;
    cmd->param2 = param2;
    cmd->image = image;
    cmd->font = font;
    cmd->str = str;

    // 初始化刷新消息
    memset(msg, 0, sizeof(refresh_msg_t));
    msg->RefreshType = REFRESH_TYPE_MANUAL;
    msg->draw_command = cmd;
    msg->msg_mem = msg;    // 记录自身内存指针
    msg->cmd_mem = cmd;    // 记录绘图命令内存指针

    // 发送消息到队列
    osStatus_t ret = osMessageQueuePut(LcdMsgQueueHandle, &msg, 0, 10);
    if (ret != osOK) {
        // 发送失败，释放内存
        vPortFree(cmd);
        vPortFree(msg);
    }

    return ret;
}           // 你要显示的文字指针


// ==================== 对外接口实现 ====================

osStatus_t lcd_draw_clear_screen(uint16_t color)
{
    return lcd_draw_send_cmd(CLEAR_SCREEN, 0, 0, color, 0, 0, 0, NULL, NULL, NULL);
}

osStatus_t lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    return lcd_draw_send_cmd(DRAW_PIXEL, COORD(x, y), 0, color, 0, 0, 0, NULL, NULL, NULL);
}

osStatus_t lcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    return lcd_draw_send_cmd(DRAW_LINE, COORD(x1, y1), COORD(x2, y2), color, 0, 0, 0, NULL, NULL, NULL);
}

osStatus_t lcd_draw_dotted_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                                uint16_t color, uint16_t interval, uint16_t dot_num)
{
    return lcd_draw_send_cmd(DRAW_DOTTED_LINE, COORD(x1, y1), COORD(x2, y2),
                             color, 0, interval, dot_num, NULL, NULL, NULL);
}

osStatus_t lcd_draw_rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                         uint16_t color, uint8_t is_filled)
{
    return lcd_draw_send_cmd(DRAW_RECT, COORD(x1, y1), COORD(x2, y2),
                             color, is_filled, 0, 0, NULL, NULL, NULL);
}

osStatus_t lcd_draw_round_rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                               uint16_t r, uint16_t color, uint8_t is_filled)
{
    return lcd_draw_send_cmd(DRAW_ROUND_RECT, COORD(x1, y1), COORD(x2, y2),
                             color, is_filled, r, 0, NULL, NULL, NULL);
}

osStatus_t lcd_draw_circle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color, uint8_t is_filled)
{
    return lcd_draw_send_cmd(DRAW_CIRCLE, COORD(x0, y0), 0, color, is_filled, r, 0, NULL, NULL, NULL);
}

osStatus_t lcd_draw_ellipse(uint16_t x0, uint16_t y0, uint16_t a, uint16_t b,
                            uint16_t color, uint8_t is_filled)
{
    return lcd_draw_send_cmd(DRAW_ELLIPSE, COORD(x0, y0), 0, color, is_filled, a, b, NULL, NULL, NULL);
}

osStatus_t lcd_draw_image(uint16_t x, uint16_t y, const ImageInfo* image)
{
    return lcd_draw_send_cmd(DRAW_IMAGE, COORD(x, y), 0, 0, 0, 0, 0, image, NULL, NULL);
}

osStatus_t lcd_draw_string(uint16_t x, uint16_t y, const char* str,
                           const FontInfo* font, uint16_t ft_color,uint16_t bg_color, int8_t spacing)
{
    return lcd_draw_send_cmd(DRAW_STRING, COORD(x, y), 0, ft_color, 0, bg_color, spacing, NULL, font, str);
}






















