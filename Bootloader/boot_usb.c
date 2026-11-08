/**
 ******************************************************************************
 * @file    boot_usb.c
 * @brief   bootloader 的 USB CDC（维护模式链路）。
 *          复用工程里已有的 USB Device 库（usbd_core / usbd_cdc / usbd_desc /
 *          usbd_conf），只实现 CDC 类回调与一个接收环形缓冲。
 *          数据流：USB 中断 -> 环形缓冲 -> Boot_Usb_Read() -> boot_proto.c 组帧。
 ******************************************************************************
 */
#include "boot.h"
#include "usbd_core.h"
#include "usbd_cdc.h"
#include "usbd_desc.h"
#include "usbd_conf.h"
#include "SerialCoreTask.h"

#define BOOT_USB_RX_SIZE    1024U
#define BOOT_USB_PACKET     64U          /* OTG_FS 的 CDC 端点包长 */

extern PCD_HandleTypeDef hpcd_USB_OTG_FS;

USBD_HandleTypeDef hUsbDeviceFS;

static uint8_t           s_rx[BOOT_USB_RX_SIZE];
static volatile uint32_t s_rx_head = 0U;      /* 中断写 */
static volatile uint32_t s_rx_tail = 0U;      /* 主循环读 */

/* 驱动收包缓冲必须和环形缓冲分开：
   否则下一个 OUT 包会直接覆盖 s_rx[0..63]，把还没被主循环取走的第一包数据冲掉，
   多包长帧（文件写入、升级包）永远凑不齐，表现就是"传大文件卡住"。 */
static uint8_t s_usb_rx[BOOT_USB_PACKET];
static volatile uint8_t  s_configured = 0U;
static volatile uint8_t  s_tx_done   = 1U;

static uint8_t s_tx[PROTO_FRAME_MAX];

/* usbd_conf.c 里初始化失败时会调用它 */
void Error_Handler(void)
{
    Boot_Panic(g_boot_stage);
}

/* ---------------------------- CDC 类回调 ---------------------------- */
static int8_t CDC_Init_FS(void)
{
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, s_tx, 0U);
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, s_usb_rx);

    /* SET_CONFIGURATION 走到这里，说明枚举完成、可以收发了 */
    s_configured = 1U;
    s_tx_done    = 1U;
    return (USBD_OK);
}

static int8_t CDC_DeInit_FS(void)
{
    s_configured = 0U;
    return (USBD_OK);
}

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
    (void)cmd;
    (void)pbuf;
    (void)length;
    return (USBD_OK);
}

static int8_t CDC_Receive_FS(uint8_t *Buf, uint32_t *Len)
{
    uint32_t n = (Len != NULL) ? *Len : 0U;

    for (uint32_t i = 0U; i < n; i++)
    {
        uint32_t next = (s_rx_head + 1U) % BOOT_USB_RX_SIZE;

        if (next == s_rx_tail)
        {
            break;                                  /* 满了就丢新数据 */
        }
        s_rx[s_rx_head] = Buf[i];
        s_rx_head = next;
    }

    (void)USBD_CDC_SetRxBuffer(&hUsbDeviceFS, s_usb_rx);
    (void)USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return (USBD_OK);
}

static int8_t CDC_TransmitCplt_FS(uint8_t *Buf, uint32_t *Len, uint8_t epnum)
{
    (void)Buf;
    (void)Len;
    (void)epnum;
    s_tx_done = 1U;
    return (USBD_OK);
}

static USBD_CDC_ItfTypeDef s_cdc_ops =
{
    CDC_Init_FS,
    CDC_DeInit_FS,
    CDC_Control_FS,
    CDC_Receive_FS,
    CDC_TransmitCplt_FS,
};

/* ---------------------------- 初始化 ---------------------------- */
bool Boot_Usb_Init(void)
{
    if (USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS) != USBD_OK)
    {
        return false;
    }
    if (USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC) != USBD_OK)
    {
        return false;
    }
    if (USBD_CDC_RegisterInterface(&hUsbDeviceFS, &s_cdc_ops) != USBD_OK)
    {
        return false;
    }
    if (USBD_Start(&hUsbDeviceFS) != USBD_OK)
    {
        return false;
    }
    return true;
}

/* USB 中断：stm32f4xx_it.c 在 bootloader 里没有，这里自己接 */
void OTG_FS_IRQHandler(void)
{
    HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
}

bool Boot_Usb_Configured(void)
{
    return s_configured != 0U;
}

/* ---------------------------- 收发 ---------------------------- */
uint16_t Boot_Usb_Read(uint8_t *out, uint16_t max)
{
    uint16_t n = 0U;

    while ((n < max) && (s_rx_tail != s_rx_head))
    {
        out[n++] = s_rx[s_rx_tail];
        s_rx_tail = (s_rx_tail + 1U) % BOOT_USB_RX_SIZE;
    }
    return n;
}

bool Boot_Usb_Send(const uint8_t *data, uint16_t len)
{
    if (!s_configured || (data == NULL) || (len == 0U) || (len > PROTO_FRAME_MAX))
    {
        return false;
    }

    /* 等上一帧发完（主机是"发一条等一条"，正常不会等太久） */
    uint32_t start = HAL_GetTick();
    while (!s_tx_done)
    {
        if ((HAL_GetTick() - start) > 500U)
        {
            return false;
        }
    }

    for (uint16_t i = 0U; i < len; i++)
    {
        s_tx[i] = data[i];
    }

    s_tx_done = 0U;
    if (USBD_CDC_SetTxBuffer(&hUsbDeviceFS, s_tx, len) != USBD_OK)
    {
        s_tx_done = 1U;
        return false;
    }
    if (USBD_CDC_TransmitPacket(&hUsbDeviceFS) != USBD_OK)
    {
        s_tx_done = 1U;
        return false;
    }

    return true;
}
