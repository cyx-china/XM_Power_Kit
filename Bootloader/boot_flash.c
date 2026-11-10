/**
 ******************************************************************************
 * @file    boot_flash.c
 * @brief   bootloader 用的极简 W25Qxx 轮询驱动（无 DMA、无 FreeRTOS）。
 *          应用里的 Flash.c 依赖信号量/DMA，bootloader 不能用，这里只保留
 *          读、页写（跨页自动拆分）、4KB 扇区擦除三种基本操作。
 ******************************************************************************
 */
#include "boot.h"
#include "stm32f4xx_hal.h"

/* ---------------------------- 指令集 ---------------------------- */
#define CMD_READ_STATUS1    0x05U
#define CMD_WRITE_ENABLE    0x06U
/* 3 字节地址（W25Q32/64/128 等 ≤16MB 的片子） */
#define CMD_READ_DATA_3B    0x03U
#define CMD_PAGE_PROGRAM_3B 0x02U
#define CMD_SECTOR_ERASE_3B 0x20U
/* 4 字节地址（W25Q256/512 等 ≥32MB 的片子，必须先发 0xB7 进入该模式） */
#define CMD_READ_DATA_4B    0x13U
#define CMD_PAGE_PROGRAM_4B 0x12U
#define CMD_SECTOR_ERASE_4B 0x21U
#define CMD_JEDEC_ID        0x9FU
#define CMD_ENABLE_4BYTE    0xB7U
#define CMD_WAKE_UP         0xABU
#define CMD_RESET_ENABLE    0x66U
#define CMD_RESET_DEVICE    0x99U

#define STATUS_BUSY         0x01U
#define STATUS_WEL          0x02U

#define PAGE_SIZE           256U
#define SECTOR_SIZE         4096U

#define FLASH_CS_LOW()      (FLASH_CS_PORT->BSRR = (uint32_t)FLASH_CS_PIN << 16)
#define FLASH_CS_HIGH()     (FLASH_CS_PORT->BSRR = (uint32_t)FLASH_CS_PIN)

#define FLASH_CS_PORT       GPIOD
#define FLASH_CS_PIN        GPIO_PIN_6

static uint32_t s_flash_size = 0U;
static bool     s_addr4byte  = false;   /* 当前是否使用 4 字节地址命令（≥32MB 的片子才为真） */

/* ---------------------------- SPI1 轮询收发 ---------------------------- */
static void spi_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

    /* PB3=SCK, PB4=MISO, PB5=MOSI，全部复用功能 AF5 */
    GPIOB->MODER   = (GPIOB->MODER & ~((3U << 6) | (3U << 8) | (3U << 10)))
                   | ((2U << 6) | (2U << 8) | (2U << 10));
    GPIOB->OTYPER &= ~((1U << 3) | (1U << 4) | (1U << 5));
    GPIOB->OSPEEDR |= ((3U << 6) | (3U << 8) | (3U << 10));
    GPIOB->PUPDR    = (GPIOB->PUPDR & ~((3U << 6) | (3U << 8) | (3U << 10)))
                    | (1U << 8);                     /* MISO 上拉 */
    GPIOB->AFR[0]   = (GPIOB->AFR[0] & ~((0xFU << 12) | (0xFU << 16) | (0xFU << 20)))
                    | ((5U << 12) | (5U << 16) | (5U << 20));

    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    SPI1->CR1 = 0U;
    SPI1->CR2 = 0U;
    /* 主机、软件片选、模式0、/4 分频（APB2 的一半） */
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_BR_0 | SPI_CR1_SPE;
}

static inline uint8_t spi_xfer(uint8_t value)
{
    while ((SPI1->SR & SPI_SR_TXE) == 0U)
    {
    }
    *(__IO uint8_t *)&SPI1->DR = value;
    while ((SPI1->SR & SPI_SR_RXNE) == 0U)
    {
    }
    return *(volatile uint8_t *)&SPI1->DR;
}

/* ---------------------------- 底层时序 ---------------------------- */
static uint8_t read_status(void)
{
    uint8_t status;
    FLASH_CS_LOW();
    (void)spi_xfer(CMD_READ_STATUS1);
    status = spi_xfer(0xFFU);
    FLASH_CS_HIGH();
    return status;
}

static bool wait_ready(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    while ((read_status() & STATUS_BUSY) != 0U)
    {
        if ((HAL_GetTick() - start) > timeout_ms)
        {
            return false;
        }
    }
    return true;
}

static bool write_enable(void)
{
    FLASH_CS_LOW();
    (void)spi_xfer(CMD_WRITE_ENABLE);
    FLASH_CS_HIGH();
    return (read_status() & STATUS_WEL) != 0U;
}

/* 按当前地址模式发送地址：4 字节模式 4 个字节，3 字节模式 3 个字节 */
static void send_addr(uint32_t addr)
{
    if (s_addr4byte)
    {
        (void)spi_xfer((uint8_t)(addr >> 24));
    }
    (void)spi_xfer((uint8_t)(addr >> 16));
    (void)spi_xfer((uint8_t)(addr >> 8));
    (void)spi_xfer((uint8_t)addr);
}

/* 软复位：把芯片恢复到上电默认状态（3 字节地址模式），清掉上一次运行残留的模式设置 */
static void reset_chip(void)
{
    FLASH_CS_LOW();
    (void)spi_xfer(CMD_RESET_ENABLE);
    FLASH_CS_HIGH();

    FLASH_CS_LOW();
    (void)spi_xfer(CMD_RESET_DEVICE);
    FLASH_CS_HIGH();

    HAL_Delay(1);                       /* tRST 约 3µs，留足余量 */
}

/* ---------------------------- 对外接口 ---------------------------- */
bool Boot_Flash_Init(void)
{
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* 片选：推挽输出（上拉），默认拉高 */
    Boot_Gpio_Setup(FLASH_CS_PORT, FLASH_CS_PIN, false, 1U, 3U, 0U);
    FLASH_CS_HIGH();

    spi_init();

    /* 唤醒（上电第一次访问前需要，掉电保持时不敏感） */
    FLASH_CS_LOW();
    (void)spi_xfer(CMD_WAKE_UP);
    FLASH_CS_HIGH();
    HAL_Delay(1);

    /* 读 JEDEC ID：EF 40 19 => W25Q256（32MB） */
    uint8_t id[3];
    FLASH_CS_LOW();
    (void)spi_xfer(CMD_JEDEC_ID);
    id[0] = spi_xfer(0xFFU);
    id[1] = spi_xfer(0xFFU);
    id[2] = spi_xfer(0xFFU);
    FLASH_CS_HIGH();

    if (id[0] == 0x00U || id[0] == 0xFFU)
    {
        return false;
    }

    /* 先软复位：芯片回到上电默认的 3 字节地址模式。
       否则上一次运行（应用侧）留下的 4 字节模式会影响后续命令的解析。 */
    reset_chip();

    if (id[2] >= 0x19U)                 /* 32MB 及以上：改用 4 字节地址命令 */
    {
        s_addr4byte = true;
        FLASH_CS_LOW();
        (void)spi_xfer(CMD_ENABLE_4BYTE);
        FLASH_CS_HIGH();
        HAL_Delay(1);
        s_flash_size = (id[2] >= 0x20U) ? (64U * 1024U * 1024U) : (32U * 1024U * 1024U);
    }
    else                                /* 4MB~16MB（W25Q32/64/128）：保持 3 字节地址 */
    {
        s_addr4byte = false;
        s_flash_size = 1UL << id[2];
    }

    return true;
}

uint32_t Boot_Flash_Size(void)
{
    return s_flash_size;
}

void Boot_Flash_Read(uint32_t addr, void *dst, uint32_t len)
{
    uint8_t *out = (uint8_t *)dst;

    FLASH_CS_LOW();
    (void)spi_xfer(s_addr4byte ? CMD_READ_DATA_4B : CMD_READ_DATA_3B);
    send_addr(addr);
    while (len-- > 0U)
    {
        *out++ = spi_xfer(0xFFU);
    }
    FLASH_CS_HIGH();
}

bool Boot_Flash_EraseSector(uint32_t addr)
{
    if (!write_enable())
    {
        return false;
    }

    FLASH_CS_LOW();
    (void)spi_xfer(s_addr4byte ? CMD_SECTOR_ERASE_4B : CMD_SECTOR_ERASE_3B);
    send_addr(addr);
    FLASH_CS_HIGH();

    return wait_ready(500U);
}

bool Boot_Flash_Write(uint32_t addr, const void *src, uint32_t len)
{
    const uint8_t *in = (const uint8_t *)src;

    while (len > 0U)
    {
        uint32_t page_left = PAGE_SIZE - (addr % PAGE_SIZE);
        uint32_t chunk = (len < page_left) ? len : page_left;

        if (!write_enable())
        {
            return false;
        }

        FLASH_CS_LOW();
        (void)spi_xfer(s_addr4byte ? CMD_PAGE_PROGRAM_4B : CMD_PAGE_PROGRAM_3B);
        send_addr(addr);
        for (uint32_t i = 0U; i < chunk; i++)
        {
            (void)spi_xfer(in[i]);
        }
        FLASH_CS_HIGH();

        if (!wait_ready(10U))
        {
            return false;
        }

        addr += chunk;
        in   += chunk;
        len  -= chunk;
    }

    return true;
}
