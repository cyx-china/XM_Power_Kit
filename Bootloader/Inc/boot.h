/**
 ******************************************************************************
 * @file    boot.h
 * @brief   OTA Bootloader 公共定义（分区地址、升级包格式、状态记录、各模块 API）
 ******************************************************************************
 */
#ifndef BOOT_H
#define BOOT_H

#include <stdint.h>
#include <stdbool.h>

#include "stm32f4xx_hal.h"

/* ============================ 分区地址 ============================ */
#define BOOT_SIZE           (48U * 1024U)      /* bootloader：sector0 */
#define BOOT_STATE_ADDR     0x0800C000U        /* OTA 状态记录：sector1 */
#define BOOT_APP_ADDR       0x08010000U        /* 应用：sector2 起 */
#define BOOT_APP_SIZE       (448U * 1024U)     /* sector2~7 共 480KB */

/* ======================== 一次性工具固件开关 ======================== */
/* 置 1：上电无条件格式化外部 Flash，随后停在维护模式（USB 可用），
   用于出厂/救援时清空整片 Flash 再重新写入文件。
   警告：这种固件每次上电都会清空外部 Flash，正常固件必须保持 0。
   编译：cmake ... -DCMAKE_C_FLAGS="-DBOOT_FORCE_FORMAT=1"，产物另存。 */
#ifndef BOOT_FORCE_FORMAT
#define BOOT_FORCE_FORMAT 0
#endif

#define BOOT_RAM_START      0x20000000U
#define BOOT_RAM_END        0x20020000U        /* 128KB SRAM */

/* 板载 LED（与应用同一颗：PC0） */
#define BOOT_LED_PORT       GPIOC
#define BOOT_LED_PIN        GPIO_PIN_0

/* ============================ 外部 Flash 上的文件 ============================ */
#define BOOT_DIR_OTA        "0:/ota"
#define BOOT_FILE_FW        "0:/ota/FW.BIN"      /* 待升级的新固件包 */
#define BOOT_FILE_PREV      "0:/ota/PREV.BIN"    /* 升级前的版本快照（回滚源） */
#define BOOT_FILE_BOOTOK    "0:/ota/BOOTOK.BIN"  /* 应用启动成功标记 */
#define BOOT_FILE_REQ       "0:/ota/REQ.BIN"     /* 升级/回滚请求（应用写、bootloader 读） */

/* ============================ OTA 状态记录 ============================ */
#define BOOT_REC_MAGIC      0x374B4D58U          /* 'XMK7' */

typedef enum
{
    BOOT_ST_IDLE        = 0U,   /* 稳定状态，可直接启动应用 */
    BOOT_ST_WAIT_BOOTOK = 1U,   /* 刚升级完，等应用写 BOOTOK 确认 */
} boot_state_e;

typedef struct
{
    uint32_t magic;          /* BOOT_REC_MAGIC */
    uint32_t serial;         /* 每次升级/回滚自增 */
    uint32_t state;          /* boot_state_e */
    uint32_t fail_count;     /* 升级后连续启动失败次数 */
    char     version[32];    /* 当前内部 Flash 里的版本串 */
    uint32_t token;          /* 已消费的 REQ 令牌 */
    uint32_t crc;            /* 覆盖本结构体 crc 之前的全部字节 */
} boot_record_t;

/* ============================ 升级包格式 ============================ */
#define BOOT_PKG_MAGIC      "XMK7OTA1"
#define BOOT_PKG_HEAD_SIZE  80U

typedef struct
{
    char     magic[8];       /* "XMK7OTA1" */
    char     version[32];    /* 固件版本串，如 "STM32F407VE.261108.Rel.0007" */
    uint32_t image_size;     /* 镜像字节数 */
    uint32_t image_crc;      /* 镜像 CRC32 */
    uint32_t head_crc;       /* 覆盖 0..47 字节的 CRC32 */
    uint32_t load_addr;      /* 目标地址，固定 BOOT_APP_ADDR */
    char     build_time[8];  /* "YYYYMMDD" */
    char     board[16];      /* 机型串 */
} boot_pkg_head_t;

#define BOOT_PKG_BOARD      "XM_Power_Kit_V7"

/* REQ.BIN 内容（8 字节，由应用写入） */
#define BOOT_OP_UPGRADE     1U
#define BOOT_OP_ROLLBACK    2U

typedef struct
{
    uint32_t token;          /* 请求令牌，bootloader 处理完写进状态记录，避免重复执行 */
    uint16_t op;             /* BOOT_OP_UPGRADE / BOOT_OP_ROLLBACK */
    uint16_t crc;            /* 覆盖前 6 字节的 CRC32 低 16 位 */
} boot_req_t;

/* BOOTOK.BIN 内容（44 字节，由应用在启动成功后写入） */
#define BOOT_BOOTOK_MAGIC   0x314B4F42U      /* 'BOK1' */

typedef struct
{
    uint32_t magic;
    char     version[32];
    uint32_t token;
    uint32_t crc;            /* 覆盖前 40 字节 */
} boot_bootok_t;

/* 升级包解析结果 / 失败原因 */
#define BOOT_PKG_OK         0U
#define BOOT_PKG_E_OPEN     1U      /* 打不开文件 */
#define BOOT_PKG_E_HEAD     2U      /* 包头非法（magic/head_crc/地址/长度/机型） */
#define BOOT_PKG_E_CRC      3U      /* 镜像 CRC 与包头不符 */
#define BOOT_PKG_E_READ     4U      /* 文件比包头声明的短 / 读失败 */
#define BOOT_PKG_E_EMPTY    5U      /* 文件长度不够一个镜像 */

typedef struct
{
    uint32_t file_size;          /* 文件总长度 */
    uint32_t data_offset;        /* 镜像数据起点（带包头=80，裸 .bin=0） */
    uint32_t image_size;         /* 镜像字节数 */
    uint32_t want_crc;           /* 期望 CRC（包头里写的；裸 .bin 就是算出来的） */
    uint32_t got_crc;            /* 实际算出来的 CRC */
    uint8_t  has_head;           /* 1=文件带 80 字节包头 */
    uint8_t  reason;             /* BOOT_PKG_E_xxx */
    char     version[32];        /* 版本串（包头里取，裸镜像从字符串里嗅出来） */
} boot_pkg_info_t;

/* 升级包解析与搬运：stage 0=擦除 1=写入 2=回读校验 */
typedef void (*boot_progress_t)(uint8_t stage, uint16_t percent);

/* 解析升级包：带包头按包头校验，裸 .bin 直接按整文件当作镜像 */
bool Boot_Pkg_Load(const char *path, boot_pkg_info_t *info);
bool Boot_Pkg_Flash(const char *path, const boot_pkg_info_t *info, boot_progress_t progress);
void Boot_Ota_Boot(void);                                                 /* 开机决策：升级 / 回滚 / 确认 */

/* 找待安装的升级包：优先 0:/ota/FW.BIN，没有就扫 /ota 下第一个 .BIN（排除 PREV/BOOTOK/REQ） */
bool Boot_FindPackage(char *path, uint32_t size);

/* ============================ SPI Flash（轮询方式） ============================ */
bool     Boot_Flash_Init(void);
uint32_t Boot_Flash_Size(void);
void     Boot_Flash_Read(uint32_t addr, void *dst, uint32_t len);
bool     Boot_Flash_Write(uint32_t addr, const void *src, uint32_t len);
bool     Boot_Flash_EraseSector(uint32_t addr);

/* ============================ FATFS ============================ */
bool     Boot_Fs_Mount(void);
bool     Boot_Fs_Format(void);      /* 卷无文件系统时现场建 FAT16（不接电脑） */

/* ============================ 出厂初始化（目录 / 字库 / 参数文件） ============================ */
#define BOOT_DIR_FONT           "0:/font"
#define BOOT_DIR_FONT_BOOT      "0:/font/bootloader"
#define BOOT_DIR_IMAGES         "0:/images"
#define BOOT_FILE_FONT          "0:/font/bootloader/boot.bin"
#define BOOT_FILE_PARAM         "0:/uparam.bin"

#define BOOT_PROV_DIRS          0x01U   /* 建过目录 */
#define BOOT_PROV_FONT          0x02U   /* 写过字库 */
#define BOOT_PROV_PARAM         0x04U   /* 写过参数文件 */

uint8_t  Boot_Storage_Provision(void);

/* 读一个用户参数的 16 位原值（uparam.bin 没有/读不出来就用编译期默认值） */
#define BOOT_PARAM_SCREEN_BRIGHT    49U     /* Screen_Brightness */
#define BOOT_PARAM_SCREEN_FLIP      51U     /* Screen_ShowFlip  0=开反色 1=关反色 */
uint16_t Boot_Storage_Param16(uint8_t index);

/* ============================ CRC32 ============================ */
void     Boot_Crc32_Init(void);
uint32_t Boot_Crc32_Update(uint32_t crc, const void *buf, uint32_t len);
uint32_t Boot_Crc32(const void *buf, uint32_t len);
uint32_t Boot_Crc32_Legacy(const void *buf, uint32_t len);   /* 兼容早期固件的非标准初值写法 */

/* ============================ 状态记录（内部 Flash sector1） ============================ */
void     Boot_State_Load(boot_record_t *rec);
bool     Boot_State_Save(const boot_record_t *rec);

/* ============================ 应用 ============================ */
bool     Boot_App_Valid(void);
void     Boot_App_Jump(void);

/* ============================ 指示灯 ============================ */
void     Boot_Led(bool on);

/* ============================ 自检/故障 ============================ */
/* 当前启动阶段（0=复位后，1=时钟，2=GPIO，3=屏幕，4=外部 Flash，5=文件系统，
   6=OTA 决策，7=跳转）。故障时 LED 会闪 (g_boot_stage+1) 次。 */
extern volatile uint8_t g_boot_stage;
/* 屏幕（FSMC + 面板）已经初始化完成，故障处理时才允许往屏幕写字/填色 */
extern volatile uint8_t g_boot_lcd_ready;

void     Boot_Panic(uint32_t stage);

/* 背光（PA9）：可以先于点屏打开，保证上电就有光 */
void     Boot_Lcd_Backlight(bool on);
/* 背光 PWM 调光（PA9 = TIM1_CH2，percent 0~100）。时钟/引脚就绪后调用才有效 */
void     Boot_Lcd_BacklightPwm(uint8_t percent);

/* ============================ GPIO 直写（省掉 HAL_GPIO_Init 的 ~430 字节） ============================ */
/* is_af：0=推挽输出 1=复用推挽；pull：0=无 1=上拉；speed：0=低速 3=最高速；af：0~15 */
void     Boot_Gpio_Setup(GPIO_TypeDef *port, uint32_t pins, bool is_af,
                         uint32_t pull, uint32_t speed, uint32_t af);

/* ============================ 内部 Flash 擦扇区（直写寄存器） ============================ */
bool     Boot_IntFlash_EraseSector(uint32_t sector);

/* ============================ 屏幕 ============================ */
bool     Boot_Lcd_Init(void);
/* 面板颜色反色：与应用 InitTask 保持一致（ShowFlip=0 → 开反色 0x21） */
void     Boot_Lcd_SetInversion(bool invert_on);
void     Boot_Lcd_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void     Boot_Lcd_Fill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void     Boot_Lcd_Blit(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pixels);

/* ============================ 字库（外部 Flash 的 BFNT 文件） ============================ */
#define BOOT_FONT_PATH  "0:/font/boot.bin"

void     Boot_Font_Init(void);
bool     Boot_Font_Ready(void);
uint16_t Boot_Font_TextWidth(const char *utf8);
void     Boot_Font_DrawText(uint16_t x, uint16_t y, const char *utf8, uint16_t fg, uint16_t bg);

/* ============================ 界面 ============================ */
extern const char Boot_Txt_Title[];
extern const char Boot_Txt_Version[];
extern const char Boot_Txt_Size[];
extern const char Boot_Txt_Crc[];
extern const char Boot_Txt_Footer[];
extern const char Boot_Txt_NoFont[];
extern const char Boot_Txt_Verify[];
extern const char Boot_Txt_Erase[];
extern const char Boot_Txt_Write[];
extern const char Boot_Txt_Check[];
extern const char Boot_Txt_Reboot[];
extern const char Boot_Txt_Rollback[];
extern const char Boot_Txt_NoPkg[];
extern const char Boot_Txt_BadPkg[];
extern const char Boot_Txt_WriteFail[];
extern const char Boot_Txt_RollbackFail[];
extern const char Boot_Txt_Wait[];
extern const char Boot_Txt_Ready[];
extern const char Boot_Txt_InitTitle[];
extern const char Boot_Txt_InitFooter[];
extern const char Boot_Txt_Formatting[];
extern const char Boot_Txt_Provision[];
extern const char Boot_Txt_SelfTest[];
extern const char Boot_Txt_Formatted[];
extern const char Boot_Txt_FormatFail[];
extern const char Boot_Txt_AllOk[];
extern const char Boot_Txt_SomeFail[];
extern const char Boot_Txt_Stay[];
extern const char Boot_Txt_StayHint[];
extern const char Boot_Txt_JumpIn[];
extern const char Boot_Txt_Entering[];
extern const char Boot_Txt_UsbWait[];
extern const char Boot_Txt_UsbOk[];
extern const char Boot_Txt_ForceHint[];

void     Boot_Ui_Screen(const char *title, const char *footer, bool with_bar);
void     Boot_Ui_ClearLine(uint16_t y);
void     Boot_Ui_Package(const char *version, uint32_t size, uint32_t crc);
void     Boot_Ui_Progress(uint16_t percent);
void     Boot_Ui_Status(const char *text, bool ok);
void     Boot_Ui_Error(const char *text);
void     Boot_Ui_Hint(const char *text, bool warn);
void     Boot_Ui_Item(uint8_t index, const char *label, const char *value, uint8_t state);
bool     Boot_Ui_HasBar(void);   /* true=当前是升级界面（自检行不再往屏幕上画） */

/* ============================ 自检项 ============================ */
#define BOOT_ITEM_CLOCK     0U
#define BOOT_ITEM_FLASH     1U
#define BOOT_ITEM_FS        2U
#define BOOT_ITEM_PARAM     3U
#define BOOT_ITEM_SENSOR    4U
#define BOOT_ITEM_APP       5U
#define BOOT_ITEM_NUM       6U

typedef enum
{
    BOOT_ITEM_PENDING = 0U,
    BOOT_ITEM_OK      = 1U,
    BOOT_ITEM_FAIL    = 2U,
} boot_item_state_e;

typedef struct
{
    const char *label;
    char        value[20];
    uint8_t     state;
} boot_item_t;

extern boot_item_t g_boot_items[BOOT_ITEM_NUM];

void     Boot_Item_Set(uint8_t index, uint8_t state, const char *value);
void     Boot_SelfTest_Begin(void);
void     Boot_SelfTest_Paint(void);   /* 把记录下来的自检结果一次画到屏幕上 */
bool     Boot_SelfTest_Run(bool fs_ok, bool did_format, uint8_t prov_flags);
bool     Boot_SelfTest_AllOk(void);

/* ============================ USB CDC 维护模式 ============================ */
bool     Boot_Usb_Init(void);
bool     Boot_Usb_Configured(void);
uint16_t Boot_Usb_Read(uint8_t *out, uint16_t max);
bool     Boot_Usb_Send(const uint8_t *data, uint16_t len);
void     Boot_Proto_Poll(void);

/* ============================ I2C 自检（只探测器件是否应答） ============================ */
bool     Boot_I2c_Init(void);
uint8_t  Boot_I2c_Probe(void);

/* ============================ 按键（PB12~15 + PE4，低有效） ============================ */
void     Boot_Key_Init(void);
bool     Boot_Key_Pressed(void);

#endif /* BOOT_H */
