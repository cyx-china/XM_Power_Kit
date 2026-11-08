/**
******************************************************************************
  * @file           : SerialLink.c
  * @brief          : 通信链路层（USB CDC / USART3 蓝牙）
  * @date           : 2026/11/08
  * @license        : CC-BY-NC-SA 4.0
  ******************************************************************************
  * USB   : CDC 虚拟串口，接收回调在 USB 中断里，发送用 CDC_Transmit_FS 轮询等待空闲。
  * 蓝牙  : USART3（PB10/PB11，115200 8N1），DMA1_Stream1 循环接收 + 空闲中断切包；
  *         发送用 TXE 中断逐字节发送，发送期间任务阻塞在信号量上，不占用 CPU。
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 雪萌_Xuemeng
  * All rights reserved.
  ******************************************************************************
  */

#include "SerialCoreTask.h"

#include <string.h>

#include "cmsis_os2.h"
#include "stm32f4xx_hal.h"
#include "usart.h"
#include "usbd_cdc_if.h"

/* ================================ USB CDC ================================ */
#define USB_TX_TIMEOUT_MS       200U

static uint16_t link_send_usb(const uint8_t *data, uint16_t len) {
    uint32_t t0 = osKernelGetTickCount();

    /* CDC_Transmit_FS 只登记缓冲区指针，数据由 USB 中断异步发出，
       必须等发送真正结束，调用方的缓冲区才能复用 */
    for (;;) {
        if (!CDC_TransmitBusy() && CDC_Transmit_FS((uint8_t *)data, len) == USBD_OK) {
            uint32_t t1 = osKernelGetTickCount();
            while (CDC_TransmitBusy()) {
                if ((uint32_t)(osKernelGetTickCount() - t1) > USB_TX_TIMEOUT_MS) {
                    return 0U;
                }
                osDelay(1U);
            }
            return len;
        }
        if ((uint32_t)(osKernelGetTickCount() - t0) > USB_TX_TIMEOUT_MS) {
            return 0U;              // USB 未连接或一直忙
        }
        osDelay(1U);
    }
}

void SerialLink_UsbRx(const uint8_t *data, uint32_t len) {
    SerialProto_Feed(LINK_USB, data, len);
}

/* ================================ 蓝牙 USART3 ================================ */
#define BT_RX_RING_SIZE         512U
#define BT_TX_TIMEOUT_MS        1000U

#define USART3_IRQ_PRIO         6U      // 必须 >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY(5)

static uint8_t           s_bt_ring[BT_RX_RING_SIZE] __attribute__((aligned(4)));
static volatile uint16_t s_bt_rx_pos;                 // DMA 已写入、尚未交给协议层的位置
static volatile uint32_t s_bt_baud = 115200U;

static volatile const uint8_t *s_bt_tx_ptr;
static volatile uint16_t s_bt_tx_len;
static volatile uint16_t s_bt_tx_pos;
static volatile uint8_t  s_bt_tx_busy;
static osSemaphoreId_t   s_bt_tx_sem;

/* 把 DMA 环形缓冲里新增的字节交给协议层（中断上下文） */
static void bt_rx_drain(void) {
    uint16_t pos = (uint16_t)(BT_RX_RING_SIZE - DMA1_Stream1->NDTR);

    if (pos == s_bt_rx_pos) {
        return;
    }
    if (pos > s_bt_rx_pos) {
        SerialProto_Feed(LINK_BT, &s_bt_ring[s_bt_rx_pos], (uint32_t)(pos - s_bt_rx_pos));
    } else {
        /* DMA 已绕回：先给尾部，再给头部 */
        SerialProto_Feed(LINK_BT, &s_bt_ring[s_bt_rx_pos], (uint32_t)(BT_RX_RING_SIZE - s_bt_rx_pos));
        SerialProto_Feed(LINK_BT, &s_bt_ring[0], (uint32_t)pos);
    }
    s_bt_rx_pos = pos;
}

/* 启动 USART3 的 DMA 循环接收与空闲中断 */
static void bt_rx_start(void) {
    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    DMA1_Stream1->CR = 0U;
    while ((DMA1_Stream1->CR & DMA_SxCR_EN) != 0U) {
        /* 等待 DMA 真正关闭 */
    }
    DMA1->LIFCR = 0x00000FC0U;                      // 清 Stream1 全部标志
    DMA1_Stream1->PAR  = (uint32_t)(&USART3->DR);
    DMA1_Stream1->M0AR = (uint32_t)s_bt_ring;
    DMA1_Stream1->NDTR = BT_RX_RING_SIZE;
    DMA1_Stream1->FCR  = 0U;
    DMA1_Stream1->CR   = (4U << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_MINC | DMA_SxCR_CIRC;
    DMA1_Stream1->CR  |= DMA_SxCR_EN;

    s_bt_rx_pos = 0U;

    USART3->CR3 |= USART_CR3_DMAR;
    USART3->CR1 |= USART_CR1_IDLEIE | USART_CR1_PEIE;

    HAL_NVIC_SetPriority(USART3_IRQn, USART3_IRQ_PRIO, 0U);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
}

/* 发送：TXE 中断逐字节送出，完成后释放信号量 */
static uint16_t link_send_bt(const uint8_t *data, uint16_t len) {
    if (s_bt_tx_sem == NULL || s_bt_tx_busy != 0U) {
        return 0U;
    }

    (void)osSemaphoreAcquire(s_bt_tx_sem, 0U);      // 清掉可能残留的完成信号
    s_bt_tx_ptr = data;
    s_bt_tx_len = len;
    s_bt_tx_pos = 0U;
    s_bt_tx_busy = 1U;

    USART3->CR1 &= ~USART_CR1_TCIE;
    USART3->CR1 |= USART_CR1_TXEIE;                 // 触发第一个 TXE

    if (osSemaphoreAcquire(s_bt_tx_sem, BT_TX_TIMEOUT_MS) != osOK) {
        USART3->CR1 &= ~(USART_CR1_TXEIE | USART_CR1_TCIE);
        s_bt_tx_busy = 0U;
        return 0U;
    }
    s_bt_tx_busy = 0U;
    return len;
}

uint32_t SerialLink_GetBtBaud(void) {
    return s_bt_baud;
}

void SerialLink_SetBtBaud(uint32_t baud) {
    if (baud < 1200U || baud > 921600U) {
        return;
    }
    s_bt_baud = baud;

    /* 直接写 BRR（OVER8 = 0），避免 HAL_UART_Init 重新配置引脚并清掉 DMAR */
    uint32_t pclk = HAL_RCC_GetPCLK1Freq();
    uint32_t div  = (pclk + (baud / 2U)) / baud;
    if (div < 16U) {
        div = 16U;
    }
    USART3->BRR = (uint16_t)div;
    huart3.Init.BaudRate = baud;
}

/* ================================ 对外接口 ================================ */
void SerialLink_Init(void) {
    if (s_bt_tx_sem == NULL) {
        s_bt_tx_sem = osSemaphoreNew(1U, 0U, NULL);
    }
    bt_rx_start();
}

uint16_t SerialLink_Send(LinkId_t link, const uint8_t *data, uint16_t len) {
    if (data == NULL || len == 0U) {
        return 0U;
    }
    if (link == LINK_USB) {
        return link_send_usb(data, len);
    }
    if (link == LINK_BT) {
        return link_send_bt(data, len);
    }
    return 0U;
}

/* ================================ USART3 中断 ================================ */
void USART3_IRQHandler(void) {
    uint32_t sr = USART3->SR;

    /* ---- 接收：空闲 / 溢出 / 噪声 / 帧错误 ---- */
    if ((sr & (USART_SR_IDLE | USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) != 0U) {
        (void)USART3->DR;                           // 先读 SR 再读 DR，清 IDLE 与错误标志
        if ((sr & USART_SR_IDLE) != 0U) {
            bt_rx_drain();
        }
    }

    /* ---- 发送 ---- */
    if (((sr & USART_SR_TXE) != 0U) && ((USART3->CR1 & USART_CR1_TXEIE) != 0U)) {
        if (s_bt_tx_pos < s_bt_tx_len) {
            USART3->DR = s_bt_tx_ptr[s_bt_tx_pos];
            s_bt_tx_pos++;
        } else {
            USART3->CR1 &= ~USART_CR1_TXEIE;
            USART3->CR1 |= USART_CR1_TCIE;
        }
    }
    if (((sr & USART_SR_TC) != 0U) && ((USART3->CR1 & USART_CR1_TCIE) != 0U)) {
        USART3->CR1 &= ~USART_CR1_TCIE;
        if (s_bt_tx_sem != NULL) {
            (void)osSemaphoreRelease(s_bt_tx_sem);
        }
    }
}
