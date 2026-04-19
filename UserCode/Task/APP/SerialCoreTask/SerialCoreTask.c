#include "SerialCoreTask.h"
#include "UserDefineManage.h"
#include <string.h>
#include "os_handles.h"
#include "usbd_cdc_if.h"
#include "UserTask.h"

// ================== 全局变量定义 ==================
static uint8_t rx_buffer[RX_BUFFER_SIZE] = {0};  // 接收缓冲区
static uint8_t tx_buffer[TX_BUFFER_SIZE] = {0};  // 发送缓冲区

volatile bool Is_Serial_Busy = false;                   // 是否正在处理数据
volatile bool IS_Receiving = false;              // 是否正在接收标志位
volatile bool Receiving_Done = false;            // 完成接收标志位
volatile uint16_t Rx_Buffer_Ptr = 0;             // 接收缓冲区指针
volatile CmdCode Current_Cmd = CMD_NULL;          // 当前正在处理的指令

// ================== 内部辅助函数：计算校验和 ==================
static uint8_t calc_checksum(uint8_t *data, uint16_t len)
{
    uint32_t sum = 0;
    for (uint16_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return (uint8_t)(sum & 0xFF);
}

// ================== 发送指令包 ==================
void Serial_Send_Cmd(CmdCode cmd)
{
    tx_buffer[0] = CMD_PACK_START;
    tx_buffer[1] = (uint8_t)cmd;
    tx_buffer[2] = calc_checksum(tx_buffer, 2);
    tx_buffer[3] = CMD_PACK_END;
    CDC_Transmit_FS(tx_buffer, 4);
}

// ================== 发送数据包 ==================
void Serial_Send_Data(uint8_t *data, uint16_t len)
{
    if (len > 255) return;

    uint16_t idx = 0;
    tx_buffer[idx++] = DATA_PACK_START;
    tx_buffer[idx++] = (uint8_t)len;
    memcpy(&tx_buffer[idx], data, len);
    idx += len;
    tx_buffer[idx] = calc_checksum(tx_buffer, idx);
    idx++;
    tx_buffer[idx++] = DATA_PACK_END;
    CDC_Transmit_FS(tx_buffer, idx);
}

// ================== CDC接收回调函数 ==================
void CB_CDC_Receive(uint8_t *Buf, uint32_t *Len)
{
    uint32_t len = *Len;
    if (len == 0 || len > 64) return;

    // ========== 未在接收数据包，先判断是不是指令包 ==========
    if (!IS_Receiving) {
        if (len == 4 && Buf[0] == CMD_PACK_START && Buf[3] == CMD_PACK_END) {
            // 是指令包，校验和验证
            if (calc_checksum(Buf, 2) == Buf[2]) {
                CmdCode cmd_code = (CmdCode)Buf[1];

                // 如果是写入指令，需要准备接收数据包
                if (cmd_code == CMD_WRITE_Data) {
                    Current_Cmd = cmd_code;
                    IS_Receiving = true;
                    Rx_Buffer_Ptr = 0;
                    Receiving_Done = false;
                }

                // 发送指令到消息队列
                osMessageQueuePut(SerialMsgQueueHandle, &cmd_code, 0, 0);
            } else {
                // 校验失败
                CmdCode err = ERROR_Verify;
                osMessageQueuePut(SerialMsgQueueHandle, &err, 0, 0);
            }
        }
        return;
    }

    // ========== 正在接收数据包 ==========
    if (IS_Receiving) {
        // 检查缓冲区是否溢出
        if (Rx_Buffer_Ptr + len > RX_BUFFER_SIZE) {
            IS_Receiving = false;
            Rx_Buffer_Ptr = 0;
            return;
        }

        // 拷贝数据到缓冲区
        memcpy(&rx_buffer[Rx_Buffer_Ptr], Buf, len);
        Rx_Buffer_Ptr += len;

        // 检查是否收到包尾
        if (Buf[len - 1] == DATA_PACK_END) {
            IS_Receiving = false;
            Receiving_Done = true;
        }
    }
}

// ================== 任务函数实现 ==================
void Start_SerialCoreTask(void const * argument)
{
    CmdCode cmd_code;
    const uint8_t version_str[8] = VERSION_STR;
    static uint8_t data_buf[PARAM_TOTAL_BYTES] = {0}; // 参数数据暂存缓冲区

    for (;;) {
        // 等待消息队列
        if (osMessageQueueGet(SerialMsgQueueHandle, &cmd_code, NULL, osWaitForever) != osOK) {
            continue;
        }
        // 检查忙标志
        if (Is_Serial_Busy && cmd_code != CMD_ACK) {
            Serial_Send_Cmd(ERROR_Busy);
            continue;
        }

        switch (cmd_code) {
            case CMD_READ_Version: {
                // 回复ACK
                Serial_Send_Cmd(CMD_ACK);
                // 简单延时
                osDelay(10);
                // 发送版本号数据包
                Serial_Send_Data((uint8_t*)version_str, 8);
                break;
            }

            case CMD_READ_CurrentData: {
                // 回复ACK
                Serial_Send_Cmd(CMD_ACK);
                osDelay(10);
                // 读取当前参数到缓冲区
                if (Process_Current_Data(data_buf) == HAL_OK) {
                    // 发送数据包
                    Serial_Send_Data(data_buf, PARAM_TOTAL_BYTES);
                }
                break;
            }

            case CMD_READ_DefaultData: {
                // 回复ACK
                Serial_Send_Cmd(CMD_ACK);
                osDelay(10);
                // 填充默认参数到缓冲区
                if (Process_Default_Data(data_buf) == HAL_OK) {
                    // 发送数据包
                    Serial_Send_Data(data_buf, PARAM_TOTAL_BYTES);
                }
                break;
            }

            case CMD_WRITE_Data: {
                // 回复ACK
                Serial_Send_Cmd(CMD_ACK);
                // 等待接收数据包（等待Receiving_Done标志）
                uint32_t timeout = 0;
                while (!Receiving_Done) {
                    osDelay(1);
                    timeout++;
                    if (timeout > 5000) { // 5秒超时
                        Receiving_Done = false;
                        Rx_Buffer_Ptr = 0;
                        return;
                    }
                }
                // 校验接收到的数据包
                if (Rx_Buffer_Ptr < 4) {
                    Receiving_Done = false;
                    Rx_Buffer_Ptr = 0;
                    Serial_Send_Cmd(ERROR_Verify);
                    return;
                }
                // 验证包头、包尾
                if (rx_buffer[0] != DATA_PACK_START || rx_buffer[Rx_Buffer_Ptr - 1] != DATA_PACK_END) {
                    Receiving_Done = false;
                    Rx_Buffer_Ptr = 0;
                    Serial_Send_Cmd(ERROR_Verify);
                    return;
                }
                // 验证数据长度
                uint8_t data_len = rx_buffer[1];
                if (data_len != PARAM_TOTAL_BYTES) {
                    Receiving_Done = false;
                    Rx_Buffer_Ptr = 0;
                    Serial_Send_Cmd(ERROR_Verify);
                    return;
                }
                // 验证校验和
                uint8_t calc_cs = calc_checksum(rx_buffer, 2 + data_len);
                if (calc_cs != rx_buffer[2 + data_len]) {
                    Receiving_Done = false;
                    Rx_Buffer_Ptr = 0;
                    Serial_Send_Cmd(ERROR_Verify);
                    return;
                }
                // 设置忙标志，写入Flash
                Is_Serial_Busy = true;
                // 写入到flash
                HAL_StatusTypeDef RES = Write_Current_Data(&rx_buffer[2], PARAM_TOTAL_BYTES);
                Is_Serial_Busy = false;

                // 回复结果
                if (RES == HAL_OK) {
                    osDelay(10);
                    UserParam_LoadAllValues();       // 将新值读取到用户参数表
                    Serial_Send_Cmd(CMD_WRITE_Data); // 回显写入指令表示成功
                } else {
                    Serial_Send_Cmd(ERROR_Verify);
                }
                // 清理标志
                Receiving_Done = false;
                Rx_Buffer_Ptr = 0;
                break;
            }

            case ERROR_Verify: {
                // 上位机报告校验错误
                break;
            }

            case ERROR_Busy: {
                // 上位机报告忙
                break;
            }

            default: {
                break;
            }
        }
    }
}