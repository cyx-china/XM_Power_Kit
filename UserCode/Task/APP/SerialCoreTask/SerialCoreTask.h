#ifndef XM_POWER_KIT_SERIAL_H
#define XM_POWER_KIT_SERIAL_H

#include <stdint.h>

// ================== 协议常量定义 ==================
#define CMD_PACK_START              0xAA     // 指令包包头
#define CMD_PACK_END                0xFF     // 指令包包尾
#define DATA_PACK_START             0xBB     // 数据包包头
#define DATA_PACK_END               0xFF     // 数据包包尾

typedef enum {
    CMD_ACK                 =       0x00,    // 应答
    CMD_READ_Version        =       0x01,    // 读取版本
    CMD_READ_CurrentData    =       0x02,    // 读取当前用户数据
    CMD_READ_DefaultData    =       0x03,    // 读取默认用户数据
    CMD_WRITE_Data          =       0x04,    // 写入当前用户数据
    ERROR_Verify            =       0xEE,    // 校验失败
    ERROR_Busy              =       0xEF,    // 系统忙
    CMD_NULL                =       0xCC     // 默认值
} CmdCode;

// ================== 配置常量 ==================
#define RX_BUFFER_SIZE              512      // 接收缓冲区大小（增大）
#define TX_BUFFER_SIZE              512      // 发送缓冲区大小

// ================== 全局函数声明 ==================
void CB_CDC_Receive(uint8_t *Buf, uint32_t *Len);
void Serial_Send_Cmd(CmdCode cmd);
void Serial_Send_Data(uint8_t *data, uint16_t len);

#endif //XM_POWER_KIT_SERIAL_H