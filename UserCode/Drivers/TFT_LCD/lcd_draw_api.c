#include "lcd_draw_api.h"
#include "freertos_os2.h"
#include "cmsis_os2.h"
#include <string.h>
#include <stddef.h>

#define COORD(x, y)  (((uint32_t)x << 16) | (uint16_t)y)

// ==================== 静态绘图命令池 ====================
// 原来每条图元都要 pvPortMalloc 两次（消息块 + 命令块），刷屏线程画完再 vPortFree 两次。
// 一次整页重绘会产生上百次分配/释放，既慢又制造堆碎片；队列满时还会静默丢掉图元。
// 现在改为固定 16 块静态命令池，块由生产者与刷屏线程通过空闲队列传递：
//   生产者 get 空闲块 -> 填好 -> put 进 LcdMsgQueue
//   刷屏线程画完 -> LcdDraw_ReleaseMsg() 把块还回空闲队列
// 池耗尽时生产者最多等待 LCD_CMD_WAIT_MS（刷屏线程优先级 High1，高于所有 APP 任务，
// 等待期间它必然能继续消费），超时才丢弃，形成自然背压。
#define LCD_CMD_BLOCK_NUM   16
#define LCD_CMD_STR_MAX     48
#define LCD_CMD_WAIT_MS     100

typedef struct {
    GFX_DrawCommand_t cmd;                  // 绘图命令
    refresh_msg_t     msg;                  // 刷新消息
    char              str[LCD_CMD_STR_MAX]; // 字符串缓冲（DRAW_STRING 用）
} LcdCmdBlock_t;

static LcdCmdBlock_t lcd_cmd_blocks[LCD_CMD_BLOCK_NUM];
static osMessageQueueId_t lcd_cmd_free_queue = NULL;

/**
 * @brief 初始化静态绘图命令池（必须在 osKernelStart() 之前调用一次）
 */
void LcdDraw_InitPool(void)
{
    if (lcd_cmd_free_queue != NULL) {
        return;
    }
    lcd_cmd_free_queue = osMessageQueueNew(LCD_CMD_BLOCK_NUM, sizeof(LcdCmdBlock_t*), NULL);
    if (lcd_cmd_free_queue == NULL) {
        return;
    }
    for (uint32_t i = 0; i < LCD_CMD_BLOCK_NUM; i++) {
        LcdCmdBlock_t* blk = &lcd_cmd_blocks[i];
        (void)osMessageQueuePut(lcd_cmd_free_queue, &blk, 0, 0);
    }
}

/**
 * @brief 把一个池块还给命令池（由刷屏线程在绘制完成后调用）
 * @param msg 该块内的 refresh_msg_t 指针
 */
void LcdDraw_ReleaseMsg(refresh_msg_t* msg)
{
    if (msg == NULL || lcd_cmd_free_queue == NULL) {
        return;
    }
    LcdCmdBlock_t* blk = (LcdCmdBlock_t*)((uint8_t*)msg - offsetof(LcdCmdBlock_t, msg));
    (void)osMessageQueuePut(lcd_cmd_free_queue, &blk, 0, 0);
}

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
    // 从静态命令池取一个空闲块（不再动态分配：避免堆碎片，也避免队列满时静默丢图元）
    if (lcd_cmd_free_queue == NULL) {
        return osError;
    }

    LcdCmdBlock_t* blk = NULL;
    if (osMessageQueueGet(lcd_cmd_free_queue, &blk, NULL, LCD_CMD_WAIT_MS) != osOK || blk == NULL) {
        return osErrorNoMemory;     // 池已耗尽（刷屏线程异常），本次图元丢弃
    }

    GFX_DrawCommand_t* cmd = &blk->cmd;
    refresh_msg_t* msg = &blk->msg;

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

    // 字符串必须拷贝到命令块自带的缓冲：
    // 消息是异步消费的，若只保存调用者的指针，调用者在绘制完成前复用缓冲区就会显示乱码。
    if (flush_type == DRAW_STRING && str != NULL) {
        size_t str_len = strlen(str);
        if (str_len >= LCD_CMD_STR_MAX) {
            str_len = LCD_CMD_STR_MAX - 1U;
        }
        memcpy(blk->str, str, str_len);
        blk->str[str_len] = '\0';
        cmd->str = blk->str;
    } else {
        cmd->str = NULL;
    }

    // 初始化刷新消息（msg_mem / cmd_mem 留空表示这是静态池块）
    memset(msg, 0, sizeof(refresh_msg_t));
    msg->RefreshType = REFRESH_TYPE_MANUAL;
    msg->draw_command = cmd;
    msg->msg_mem = NULL;
    msg->cmd_mem = NULL;

    // 发送消息到队列（队列深度 >= 池块数，必然成功；万一失败把块还回池里）
    osStatus_t ret = osMessageQueuePut(LcdMsgQueueHandle, &msg, 0, 0);
    if (ret != osOK) {
        (void)osMessageQueuePut(lcd_cmd_free_queue, &blk, 0, 0);
    }

    return ret;
}


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

// 关于此函数，有无奈之举：此函数用于向选定区域发送已知数据，由于是异步处理，所以要求数据在处理完前都不能更改。
//  这个函数我只用在了DSO这个APP上，用于刷新波形数据。由于是一列一列的刷新，需要大量的调用。
//  方法一就是给每一次调用都动态分配一个400字节的内存，但我在短时间内会调用300次，也就是120K内存，显然无法实现。
//  所以我用了方法二：发送后阻塞等待"本次刷新完成"（信号量同步），调用者可以安全复用数据缓冲区。
//  注意：等待的是本次任务完成的信号，而不是空转查询标志位，不会白占 CPU。

osStatus_t lcd_draw_data(uint16_t x0,uint16_t y0,uint16_t x1,uint16_t y1,uint16_t *data,uint16_t len) {
    // 先清掉上一次可能残留的完成信号（例如上一次等待超时），保证本次等待的是本次传输
    if (DrawData_Done_SemHandle != NULL) {
        osSemaphoreAcquire(DrawData_Done_SemHandle, 0);
    }

    uint32_t addr = (uint32_t)data;
    uint16_t addr_h = (uint16_t)(addr >> 16);  // 高16位
    uint16_t addr_l = (uint16_t)(addr & 0xFFFF); // 低16位
    osStatus_t ret = lcd_draw_send_cmd(DRAW_DATA, COORD(x0, y0), COORD(x1, y1),
                                       len, 0, addr_h, addr_l, NULL, NULL, NULL);
    if (ret != osOK) {
        return ret;     // 入队失败，数据没有被使用，可直接返回
    }

    // 阻塞等待刷屏线程完成本次 DMA 传输（最多1s，避免异常情况下永久卡死）
    if (DrawData_Done_SemHandle != NULL) {
        osSemaphoreAcquire(DrawData_Done_SemHandle, 1000);
    }
    return osOK;
}

osStatus_t lcd_draw_IsoscelesTriangle(uint16_t x,uint16_t y,Triangle_Direction_e direction,uint16_t hight,uint16_t color) {
    return lcd_draw_send_cmd(DRAW_IsoscelesTriangle, COORD(x, y), 0, color, 0, hight, direction, NULL, NULL, NULL);
}


















