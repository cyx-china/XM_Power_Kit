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
#define BOOT_FILE_GOOD      "0:/ota/GOOD.BIN"    /* 当前可运行版本的快照（回滚源） */
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
    char     version[16];    /* 当前内部 Flash 里的版本串 */
    uint32_t token;          /* 已消费的 REQ 令牌 */
    uint32_t crc;            /* 覆盖本结构体 crc 之前的全部字节 */
} boot_record_t;

/* ============================ 升级包格式 ============================ */
#define BOOT_PKG_MAGIC      "XMK7OTA1"
#define BOOT_PKG_HEAD_SIZE  64U

typedef struct
{
    char     magic[8];       /* "XMK7OTA1" */
    char     version[16];    /* 固件版本串，如 "STM32F407VE.261108.Rel.0007" */
    uint32_t image_size;     /* 镜像字节数 */
    uint32_t image_crc;      /* 镜像 CRC32 */
    uint32_t head_crc;       /* 覆盖 0..31 字节的 CRC32 */
    uint32_t load_addr;      /* 目标地址，固定 BOOT_APP_ADDR */
    char     build_time[8];  /* "YYYYMMDDHHMM" */
    char     board[16];      /* 机型串 */
} boot_pkg_head_t;

/* REQ.BIN 内容（8 字节） */
typedef struct
{
    uint32_t token;          /* 请求令牌，bootloader 处理完写回状态记录避免重复执行 */
    uint16_t op;             /* 1=升级 FW.BIN，2=回滚 GOOD.BIN */
    uint16_t crc;
} boot_req_t;

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

#endif /* BOOT_H */
