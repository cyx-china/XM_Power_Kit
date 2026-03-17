#include "ST7789.h"

#include "cmsis_os2.h"
#include "dma.h"
#include "os_handles.h"

// -------------------------- FSMC传输函数 -------------------------- //
// 发送一个16bit命令
void LCD_WR_CMD(uint16_t command) {
    *LCD_CMD_ADDR = command;
}

// 发送一个16bit数据
void LCD_WR_DATA(uint16_t data) {
    *LCD_DATA_ADDR = data;
}

// 从LCD数据口读取16bit数据
static __INLINE uint16_t LCD_RD_DATA(void) {
    return *LCD_DATA_ADDR;
}

// 读取某个点的数据

// 这个函数的数据提取部分我没搞懂，但现在能用哈哈
uint16_t LCD_ReadPoint(uint16_t Xpos, uint16_t Ypos)
{
    LCD_WR_CMD(0x3A); LCD_WR_DATA(0x66);   // 开启18bit模式，手册要求的，但我测试下来不开也行
    // 设置列地址
    LCD_WR_CMD(0x2A);
    LCD_WR_DATA(Xpos >> 8);    // X起始地址高8位
    LCD_WR_DATA(Xpos & 0xFF);  // X起始地址低8位
    LCD_WR_DATA(Xpos >> 8);    // X结束地址高8位
    LCD_WR_DATA(Xpos & 0xFF);  // X结束地址低8位

    // 设置行地址
    LCD_WR_CMD(0x2B);
    LCD_WR_DATA(Ypos >> 8);    // Y起始地址高8位
    LCD_WR_DATA(Ypos & 0xFF);  // Y起始地址低8位
    LCD_WR_DATA(Ypos >> 8);    // Y结束地址高8位
    LCD_WR_DATA(Ypos & 0xFF);  // Y结束地址低8位

    // 发送GRAM读命令
    LCD_WR_CMD(0x2E);

    // dummy
    uint16_t dummy = LCD_RD_DATA();
    (void)dummy;  // 消除未使用变量警告

    // 读取有效18bit RGB666数据
    uint16_t data_h = LCD_RD_DATA();
    uint16_t data_l = LCD_RD_DATA();

    // 从18bit RGB666提取分量，转换为16bit RGB565
    uint16_t r = (data_h >> 11) & 0x1F;  // R取高5位 (R5~R1)，适配RGB565
    uint16_t g = ((data_h & 0x03F0) >> 4) | ((data_l >> 12) & 0x0F);
    uint16_t b = (data_l >> 11) & 0x1F;  // B取高5位 (B5~B1)，适配RGB565

    // 拼接为RGB565格式
    uint16_t rgb565 = (r << 11) | (g << 5) | b;

    LCD_WR_CMD(0x3A); LCD_WR_DATA(0x05);      // 回到16bit模式

    return rgb565;
}


// 通过DMA发送大量数据
void LCD_FLUSH_DMA(uint32_t src_addr,uint32_t data_length) {

    // 开启DMA传输 （DMA开启后是直接返回，运行下一行代码的）
    HAL_DMA_Start_IT(&hdma_memtomem_dma2_stream6,src_addr,(uint32_t)LCD_DATA_ADDR,data_length);
    // 拿取信号量 （由于此时信号量为0，任务就会进入阻塞态，直到中断回调函数释放信号量）（这里就相当于等待DMA传输完成了）
    osSemaphoreAcquire(LCD_DMA_Cplt_SemHandle, osWaitForever);

}

// FSMC的DMA传输完成中断回调
void CB_LCD_FSMC_TransmitCpltCallback(DMA_HandleTypeDef *_hdma) {
    // 释放信号量
    osSemaphoreRelease(LCD_DMA_Cplt_SemHandle);
}


// -------------------------- ST7789基础函数 -------------------------- //
HAL_StatusTypeDef LCD_Init(void) {
    // 注册DMA回调函数
    HAL_DMA_RegisterCallback(&hdma_memtomem_dma2_stream6,HAL_DMA_XFER_CPLT_CB_ID,CB_LCD_FSMC_TransmitCpltCallback);
    // 硬件复位
    LCD_RST_HIGH();HAL_Delay(10);
    LCD_RST_LOW();HAL_Delay(5);
    LCD_RST_HIGH();HAL_Delay(10);

        // ST7789初始化命令
    // ----------------------- 帧速率与颜色模式 -----------------------
    LCD_WR_CMD(0x3A);        // 像素格式设置
    LCD_WR_DATA(0x05);       // 16位RGB565模式（65K色）

    LCD_WR_CMD(0xC5);        // VCOM控制
    LCD_WR_DATA(0x1A);

    LCD_WR_CMD(0x36);        // 显示方向设置（MY/MX/MV/ML控制）
    #if (DIS_DIR == 0)       // 正常竖显（MY=0,MX=0,MV=0,ML=0）
        LCD_WR_DATA(0x00);
    #elif (DIS_DIR == 1)     // 旋转90°（MY=1,MX=0,MV=1,ML=0 → 0xA0）
        LCD_WR_DATA(0xA0);
    #elif (DIS_DIR == 2)     // 旋转180°（MY=1,MX=1,MV=0,ML=0 → 0xC0）
        LCD_WR_DATA(0xC0);
    #elif (DIS_DIR == 3)     // 旋转270°（MY=0,MX=1,MV=1,ML=0 → 0x60）
        LCD_WR_DATA(0x60);
    #endif

    // -----------------------  porch设置与栅极控制 -----------------------
    LCD_WR_CMD(0xB2);        // Porch设置
    LCD_WR_DATA(0x0C);
    LCD_WR_DATA(0x0C);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(0x33);
    LCD_WR_DATA(0x33);

    LCD_WR_CMD(0xB7);        // Gate控制
    LCD_WR_DATA(0x55);       // 栅极电压设置

    // ----------------------- 电源设置 -----------------------
    LCD_WR_CMD(0xBB);        // VCOM设置
    LCD_WR_DATA(0x1A);

    LCD_WR_CMD(0xC0);        // 电源控制1
    LCD_WR_DATA(0x2C);

    LCD_WR_CMD(0xC2);        // VDV和VRH命令使能
    LCD_WR_DATA(0x01);

    LCD_WR_CMD(0xC3);        // VRH设置
    LCD_WR_DATA(0x19);

    LCD_WR_CMD(0xC4);        // VDV设置（0V）
    LCD_WR_DATA(0x20);

    LCD_WR_CMD(0xC6);        // 正常模式帧速率控制
    LCD_WR_DATA(0x0F);       // 111Hz帧速率

    LCD_WR_CMD(0xD0);
    LCD_WR_DATA(0xA7);
    LCD_WR_CMD(0xD0);        // 电源控制1
    LCD_WR_DATA(0xA4);
    LCD_WR_DATA(0xA1);

    LCD_WR_CMD(0xD6);
    LCD_WR_DATA(0xA1);

    LCD_WR_CMD(0xE8);        // 电源控制2
    LCD_WR_DATA(0x03);

    LCD_WR_CMD(0xE9);        // 均衡时间控制
    LCD_WR_DATA(0x09);
    LCD_WR_DATA(0x09);
    LCD_WR_DATA(0x08);

    // ----------------------- 伽马校正 -----------------------
    LCD_WR_CMD(0xE0);        // 正伽马设置
    LCD_WR_DATA(0xF0);
    LCD_WR_DATA(0x03);
    LCD_WR_DATA(0x09);
    LCD_WR_DATA(0x0B);
    LCD_WR_DATA(0x0A);
    LCD_WR_DATA(0x16);
    LCD_WR_DATA(0x2B);
    LCD_WR_DATA(0x33);
    LCD_WR_DATA(0x41);
    LCD_WR_DATA(0x38);
    LCD_WR_DATA(0x14);
    LCD_WR_DATA(0x14);
    LCD_WR_DATA(0x29);
    LCD_WR_DATA(0x2F);

    LCD_WR_CMD(0xE1);        // 负伽马设置
    LCD_WR_DATA(0xF0);
    LCD_WR_DATA(0x04);
    LCD_WR_DATA(0x06);
    LCD_WR_DATA(0x09);
    LCD_WR_DATA(0x08);
    LCD_WR_DATA(0x04);
    LCD_WR_DATA(0x2B);
    LCD_WR_DATA(0x32);
    LCD_WR_DATA(0x41);
    LCD_WR_DATA(0x36);
    LCD_WR_DATA(0x12);
    LCD_WR_DATA(0x12);
    LCD_WR_DATA(0x2A);
    LCD_WR_DATA(0x30);

    // ----------------------- 显示使能 -----------------------
    //LCD_WR_CMD(0x21);        // 显示反显  <-- 请按照实际需求选择是否启用
    LCD_WR_CMD(0x11);        // 退出睡眠模式（SLPOUT）
    HAL_Delay(120);                   // 睡眠退出后等待≥120ms（手册要求）
    LCD_WR_CMD(0x29);        // 显示使能（DISPON）

    return HAL_OK;
}

void LCD_SET_WINDOWS(uint16_t X, uint16_t Y, uint16_t X_END, uint16_t Y_END) {
    // 设置列地址 0x2A
    LCD_WR_CMD(0x2A);
    LCD_WR_DATA(X >> 8);        // 列起始地址高8位
    LCD_WR_DATA(X & 0xFF);      // 列起始地址低8位
    LCD_WR_DATA(X_END >> 8);    // 列结束地址高8位
    LCD_WR_DATA(X_END & 0xFF);  // 列结束地址低8位

    // 设置行地址 0x2B
    LCD_WR_CMD(0x2B);
    LCD_WR_DATA(Y >> 8);        // 行起始地址高8位
    LCD_WR_DATA(Y & 0xFF);      // 行起始地址低8位
    LCD_WR_DATA(Y_END >> 8);    // 行结束地址高8位
    LCD_WR_DATA(Y_END & 0xFF);  // 行结束地址低8位

    // 开始写GRAM 0x2C
    LCD_WR_CMD(0x2C);
}

void LCD_SetPoint(uint16_t Xpos, uint16_t Ypos) {
    LCD_SET_WINDOWS(Xpos, Ypos, Xpos, Ypos);  // 起始和结束坐标相同（单个点）
}

void LCD_Clear(uint16_t Color) {
    uint32_t total_pixels = LCD_COLUMN_COUNT * LCD_LINE_COUNT; // 总像素数
    uint32_t i;

    // 设置全屏窗口
    LCD_SET_WINDOWS(0, 0, LCD_COLUMN_COUNT - 1, LCD_LINE_COUNT - 1);

    // 批量填充
    for (i = 0; i < total_pixels; i++) {
        LCD_WR_DATA(Color);
    }
}


