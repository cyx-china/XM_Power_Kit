/**
 ******************************************************************************
 * @file    boot_selftest.c
 * @brief   开机自检表（时钟 / 外部 Flash / 文件系统 / 配置文件 / 传感器 / 应用固件）
 *          以及板载按键读取（PB12~15、PE4，低有效）。
 ******************************************************************************
 */
#include "boot.h"
#include "ff.h"

boot_item_t g_boot_items[BOOT_ITEM_NUM] =
{
    { "时钟",      "", BOOT_ITEM_PENDING },
    { "外部Flash", "", BOOT_ITEM_PENDING },
    { "文件系统",  "", BOOT_ITEM_PENDING },
    { "配置文件",  "", BOOT_ITEM_PENDING },
    { "传感器",    "", BOOT_ITEM_PENDING },
    { "应用固件",  "", BOOT_ITEM_PENDING },
};

#define VALUE_MAX 20U

static void text_copy(char *dst, const char *src)
{
    uint32_t i = 0U;

    while ((src[i] != '\0') && (i < (VALUE_MAX - 1U)))
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static uint32_t u32_text(char *out, uint32_t value)
{
    char tmp[11];
    uint32_t n = 0U;
    uint32_t i = 0U;

    if (value == 0U)
    {
        out[0] = '0';
        out[1] = '\0';
        return 1U;
    }

    while ((value > 0U) && (n < sizeof(tmp)))
    {
        tmp[n++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    while (n > 0U)
    {
        out[i++] = tmp[--n];
    }
    out[i] = '\0';

    return i;
}

void Boot_Item_Set(uint8_t index, uint8_t state, const char *value)
{
    if (index >= BOOT_ITEM_NUM)
    {
        return;
    }

    /* 只记录，不画屏：正常启动时全程不出图，出问题时由 Boot_SelfTest_Paint() 一次画全 */
    g_boot_items[index].state = state;
    text_copy(g_boot_items[index].value, (value != NULL) ? value : "");
}

void Boot_SelfTest_Begin(void)
{
    for (uint8_t i = 0U; i < BOOT_ITEM_NUM; i++)
    {
        Boot_Item_Set(i, BOOT_ITEM_PENDING, "");
    }
}

void Boot_SelfTest_Paint(void)
{
    for (uint8_t i = 0U; i < BOOT_ITEM_NUM; i++)
    {
        Boot_Ui_Item(i, g_boot_items[i].label, g_boot_items[i].value, g_boot_items[i].state);
    }
}

/**
 * @brief  跑一遍自检并逐行刷新屏幕
 * @param  fs_ok      文件系统可用（已经挂载）
 * @param  did_format 本次启动格式化过卷
 * @param  prov_flags Boot_Storage_Provision() 的返回值
 * @return 除"应用固件"外是否全部通过
 */
bool Boot_SelfTest_Run(bool fs_ok, bool did_format, uint8_t prov_flags)
{
    char     buf[VALUE_MAX];
    FILINFO  info;
    uint32_t used;

    /* ---- 时钟 ---- */
    uint32_t mhz = SystemCoreClock / 1000000U;
    used = u32_text(buf, mhz);
    text_copy(&buf[used], " MHz");
    Boot_Item_Set(BOOT_ITEM_CLOCK, (mhz >= 100U) ? BOOT_ITEM_OK : BOOT_ITEM_FAIL, buf);

    /* ---- 外部 Flash ---- */
    uint32_t mb = Boot_Flash_Size() / (1024U * 1024U);
    if (mb > 0U)
    {
        used = u32_text(buf, mb);
        text_copy(&buf[used], " MB");
        Boot_Item_Set(BOOT_ITEM_FLASH, BOOT_ITEM_OK, buf);
    }
    else
    {
        Boot_Item_Set(BOOT_ITEM_FLASH, BOOT_ITEM_FAIL, "失败");
    }

    /* ---- 文件系统 ---- */
    if (!fs_ok)
    {
        Boot_Item_Set(BOOT_ITEM_FS, BOOT_ITEM_FAIL, "失败");
    }
    else
    {
        Boot_Item_Set(BOOT_ITEM_FS, BOOT_ITEM_OK, did_format ? "已格式化" : "已挂载");
    }

    /* ---- 配置文件 ---- */
    if (!fs_ok)
    {
        Boot_Item_Set(BOOT_ITEM_PARAM, BOOT_ITEM_FAIL, "失败");
    }
    else if (f_stat(BOOT_FILE_PARAM, &info) != FR_OK)
    {
        Boot_Item_Set(BOOT_ITEM_PARAM, BOOT_ITEM_FAIL, "失败");
    }
    else
    {
        Boot_Item_Set(BOOT_ITEM_PARAM, BOOT_ITEM_OK,
                      ((prov_flags & BOOT_PROV_PARAM) != 0U) ? "已创建" : "就绪");
    }

    /* ---- 传感器（I2C 探测 3 个器件） ---- */
    (void)Boot_I2c_Init();
    uint8_t found = Boot_I2c_Probe();
    if (found >= 3U)
    {
        text_copy(buf, "3/3");
        Boot_Item_Set(BOOT_ITEM_SENSOR, BOOT_ITEM_OK, buf);
    }
    else
    {
        used = u32_text(buf, found);
        text_copy(&buf[used], "/3");
        Boot_Item_Set(BOOT_ITEM_SENSOR, BOOT_ITEM_FAIL, buf);
    }

    /* ---- 应用固件 ---- */
    Boot_Item_Set(BOOT_ITEM_APP, Boot_App_Valid() ? BOOT_ITEM_OK : BOOT_ITEM_FAIL,
                  Boot_App_Valid() ? "有效" : "无");

    return Boot_SelfTest_AllOk();
}

bool Boot_SelfTest_AllOk(void)
{
    for (uint8_t i = 0U; i < BOOT_ITEM_NUM; i++)
    {
        if (g_boot_items[i].state != BOOT_ITEM_OK)
        {
            return false;
        }
    }
    return true;
}

/* ============================ 按键 ============================ */
void Boot_Key_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    /* 上拉输入，按下为低（必须用输入模式，不能用 Boot_Gpio_Setup） */
    Boot_Gpio_Input(GPIOB, GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15, 1U);
    Boot_Gpio_Input(GPIOE, GPIO_PIN_4, 1U);
}

bool Boot_Key_Pressed(void)
{
    if ((GPIOB->IDR & (GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15)) !=
        (GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15))
    {
        return true;
    }

    return (GPIOE->IDR & GPIO_PIN_4) == 0U;
}
