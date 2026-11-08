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
#define BOOT_SIZE           (16U * 1024U)      /* bootloader：sector0 */
#define BOOT_STATE_ADDR     0x08004000U        /* OTA 状态记录：sector1 */
#define BOOT_APP_ADDR       0x08008000U        /* 应用：sector2 起 */
#define BOOT_APP_SIZE       (480U * 1024U)     /* sector2~7 共 480KB */

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

/* 升级包解析与搬运：stage 0=擦除 1=写入 2=回读校验 */
typedef void (*boot_progress_t)(uint8_t stage, uint16_t percent);

bool Boot_Pkg_Load(const char *path, boot_pkg_head_t *head);              /* 读头 + 校验 CRC */
bool Boot_Pkg_Flash(const char *path, const boot_pkg_head_t *head, boot_progress_t progress);
void Boot_Ota_Boot(void);                                                 /* 开机决策：升级 / 回滚 / 确认 */

/* ============================ SPI Flash（轮询方式） ============================ */
bool     Boot_Flash_Init(void);
uint32_t Boot_Flash_Size(void);
void     Boot_Flash_Read(uint32_t addr, void *dst, uint32_t len);
bool     Boot_Flash_Write(uint32_t addr, const void *src, uint32_t len);
bool     Boot_Flash_EraseSector(uint32_t addr);

/* ============================ FATFS ============================ */
bool     Boot_Fs_Mount(void);

/* ============================ CRC32 ============================ */
void     Boot_Crc32_Init(void);
uint32_t Boot_Crc32_Update(uint32_t crc, const void *buf, uint32_t len);
uint32_t Boot_Crc32(const void *buf, uint32_t len);

/* ============================ 状态记录（内部 Flash sector1） ============================ */
void     Boot_State_Load(boot_record_t *rec);
bool     Boot_State_Save(const boot_record_t *rec);

/* ============================ 应用 ============================ */
bool     Boot_App_Valid(void);
void     Boot_App_Jump(void);

/* ============================ 指示灯 ============================ */
void     Boot_Led(bool on);

/* ============================ GPIO 直写（省掉 HAL_GPIO_Init 的 ~430 字节） ============================ */
/* is_af：0=推挽输出 1=复用推挽；pull：0=无 1=上拉；speed：0=低速 3=最高速；af：0~15 */
void     Boot_Gpio_Setup(GPIO_TypeDef *port, uint32_t pins, bool is_af,
                         uint32_t pull, uint32_t speed, uint32_t af);

/* ============================ 内部 Flash 擦扇区（直写寄存器） ============================ */
bool     Boot_IntFlash_EraseSector(uint32_t sector);

/* ============================ 屏幕 ============================ */
bool     Boot_Lcd_Init(void);
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

void     Boot_Ui_Init(bool font_ok);
void     Boot_Ui_ClearLine(uint16_t y);
void     Boot_Ui_Package(const char *version, uint32_t size, uint32_t crc);
void     Boot_Ui_Progress(uint16_t percent);
void     Boot_Ui_Status(const char *text, bool ok);
void     Boot_Ui_Error(const char *text);

#endif /* BOOT_H */
