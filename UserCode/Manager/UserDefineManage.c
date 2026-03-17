/*
 * 这个文件很重要，它专门用来管理用户自定义的项目
 * 实际的值会存储在外置的flash中，所以请务必确保您使用的是W25Q256或着更大容量的设备，以免寻址异常而无法正常存储自定义数据
 * 同时配套有修改指定参数的函数
 */
#include "UserDefineManage.h"
#include "stm32f4xx_hal.h"
#include "UserTask.h"
#include "os_handles.h"
#include "freertos_os2.h"
#include "ff.h"  // FATFS核心头文件（必须包含，否则FIL/FRESULT等未定义）

// -------------------------- 全局变量定义 --------------------------
// 用户自定义参数默认值表
const UserParamType_t UserParamDefault = {
    // 数控电源APP - 校准参数
    .DPS_Voltage_Original        = 0,          // 电压原点
    .DPS_Voltage_Factor          = 11.09756f,  // 电阻分压比，默认是91K/8.2K
    .DPS_Voltage_DAC_Coefficient = -102.09f,   // DAC系数默认值
    .DPS_Voltage_DAC_Constant    = 4072.2f,    // DAC常数项默认值
    .DPS_Current_Original        = 0,          // 电流原点
    .DPS_Current_Factor          = 4.0f,       // 用于确定I = U * K中K的值;K = 1 / 电阻R / TP181放大倍率
    // 数控电源APP - PID参数
    .DPS_Loop_P                  = 40.0f,      // CC模式下，PID的P参数
    .DPS_Loop_I                  = 30.0f,      // I参数
    .DPS_Loop_D                  = 2.0f,       // D参数
    // 数控电源APP - 快速设置
    .DPS_Fs_Voltage_1            = 4.2f,       // 快捷设置1的电压
    .DPS_Fs_Current_1            = 1.0f,       // 电流1
    .DPS_Fs_Voltage_2            = 5.0f,       // 快捷设置2的电压
    .DPS_Fs_Current_2            = 2.0f,       // 电流2
    .DPS_Fs_Voltage_3            = 12.0f,      // 快捷设置3的电压
    .DPS_Fs_Current_3            = 3.0f,       // 电流3
    .DPS_Fs_Voltage_4            = 24.0f,      // 快捷设置4的电压
    .DPS_Fs_Current_4            = 3.0f,       // 电流4
    .DPS_Fs_Voltage_5            = 36.0f,      // 快捷设置5的电压
    .DPS_Fs_Current_5            = 10.0f,      // 电流5
    // 数控电源APP - 杂项
    .DPS_Fan_Start               = 40,         // 风扇启转温度，只支持整数
    // 示波器APP - 校准参数
    .OSC_Original                = 0,          // 示波器电压原点，正常是1.65V，这里表DAC值的偏移
    .OSC_Factor                  = 5.0f,       // 示波器5倍降压的降压比例
    .OSC_AMP_X2                  = 2.0f,       // 程控放大器倍率
    .OSC_AMP_X4                  = 4.0f,       // 程控放大器倍率
    .OSC_AMP_X8                  = 8.0f,       // 程控放大器倍率
    .OSC_AMP_X16                 = 16.0f,      // 程控放大器倍率
    .OSC_AMP_X32                 = 32.0f,      // 程控放大器倍率
    .OSC_AMP_X64                 = 64.0f,      // 程控放大器倍率
    .OSC_AMP_X128                = 128.0f,     // 程控放大器倍率
    // 信号发生器APP - 校准参数
    .DDS_Original                = 0,          // 波形发生器电压原点
    .DDS_Factor                  = 3.0f,       // 波形发生器放大倍数
    // 万用表APP - 电压表校准参数
    .DMM_Voltage_Original        = 0,          // 万用表电压档电压原点
    .DMM_Voltage_Factor_B        = 0.044715f,  // 差分放大器黑表笔的分压倍率，即输出电压 = 输入电压 * 0.0046809 （47K+2.2K分压）
    .DMM_Voltage_Factor_R        = 0.044715f,  // 红表笔的分压倍率
    // 万用表APP - 电流表校准参数
    .DMM_Current_Original        = 0,          // 电流表电流零点
    .DMM_Current_Factor          = 50.0f,      // 电流表放大倍率，也是TP181,默认50倍
    // 万用表APP - 电阻表校准参数
    .DMM_Res_Voltage             = 3.3f,       // 电阻表的基准电源，这个基本不用改
    .DMM_Res_Parasitic           = 0.0f,       // 电阻表寄生电阻（并在被测电阻上，大概170K）
    .DMM_Res_R200                = 200.0f,     // 200Ω基准电阻
    .DMM_Res_R2K                 = 2000.0f,    // 2KΩ基准电阻
    .DMM_Res_R100k               = 100000.0f,  // 100KΩ基准电阻
    // 其他配置
    .Screen_Brightness    = 100,        // 屏幕启动亮度亮度，值10~100，免得有呆子搞个0下去屏幕都看不清
    .Screen_Sleeptime     = 0,          // 休眠时间           (uint16_t)  0~3600 s
    .Screen_ShowFlip      = 0,          // 显示翻转           (uint16_t)  0/1
    .Beezer_Volume        = 100,        // 蜂鸣器音量，这个基本不用改，蜂鸣器又不吵
    .Beezer_Time          = 1,          // 蜂鸣时常，注意，实际时常是10 * 此值 ms，也不必改太，10ms足够了
    .Fan_Enable           = 1,          // 是否启用风扇        (uint16_t)  0/1
    .Fan_StartTemperture  = 40,         // 风扇启转温度        (uint16_t)  20~50
};

// 实际参数值结构体
UserParamType_t UserParam = {0};

// -------------------------- 宏定义 --------------------------
#define PARAM_PATH              "0:/uparam.bin"        // 配置文件的路径
#define PARAM_TOTAL_BYTES       (Param_Number * 4)     // 配置文件总字节数（每个参数占4字节）

UINT bw;

// -------------------------- 优化核心：参数描述表 --------------------------
// 参数描述结构体：存储每个参数的长度、默认值指针、当前值指针
typedef struct {
    uint8_t data_len;          // 数据长度：2字节(int16_t/uint16_t) 或 4字节(float)
    const void *default_ptr;   // 指向默认值结构体成员的指针
    void *current_ptr;         // 指向当前值结构体成员的指针
} ParamDesc_t;

// 按UserParamType_e枚举顺序排列的参数描述表（与枚举一一对应，不可打乱顺序）
static const ParamDesc_t param_desc_table[Param_Number] = {
    // 数控电源APP - 校准参数
    {2, &UserParamDefault.DPS_Voltage_Original,        &UserParam.DPS_Voltage_Original},
    {4, &UserParamDefault.DPS_Voltage_Factor,          &UserParam.DPS_Voltage_Factor},
    {4, &UserParamDefault.DPS_Voltage_DAC_Coefficient, &UserParam.DPS_Voltage_DAC_Coefficient},
    {4, &UserParamDefault.DPS_Voltage_DAC_Constant,    &UserParam.DPS_Voltage_DAC_Constant},
    {2, &UserParamDefault.DPS_Current_Original,        &UserParam.DPS_Current_Original},
    {4, &UserParamDefault.DPS_Current_Factor,          &UserParam.DPS_Current_Factor},
    {4, &UserParamDefault.DPS_Loop_P,                  &UserParam.DPS_Loop_P},
    {4, &UserParamDefault.DPS_Loop_I,                  &UserParam.DPS_Loop_I},
    {4, &UserParamDefault.DPS_Loop_D,                  &UserParam.DPS_Loop_D},
    {4, &UserParamDefault.DPS_Fs_Voltage_1,            &UserParam.DPS_Fs_Voltage_1},
    {4, &UserParamDefault.DPS_Fs_Current_1,            &UserParam.DPS_Fs_Current_1},
    {4, &UserParamDefault.DPS_Fs_Voltage_2,            &UserParam.DPS_Fs_Voltage_2},
    {4, &UserParamDefault.DPS_Fs_Current_2,            &UserParam.DPS_Fs_Current_2},
    {4, &UserParamDefault.DPS_Fs_Voltage_3,            &UserParam.DPS_Fs_Voltage_3},
    {4, &UserParamDefault.DPS_Fs_Current_3,            &UserParam.DPS_Fs_Current_3},
    {4, &UserParamDefault.DPS_Fs_Voltage_4,            &UserParam.DPS_Fs_Voltage_4},
    {4, &UserParamDefault.DPS_Fs_Current_4,            &UserParam.DPS_Fs_Current_4},
    {4, &UserParamDefault.DPS_Fs_Voltage_5,            &UserParam.DPS_Fs_Voltage_5},
    {4, &UserParamDefault.DPS_Fs_Current_5,            &UserParam.DPS_Fs_Current_5},
    {2, &UserParamDefault.DPS_Fan_Start,               &UserParam.DPS_Fan_Start},
    // 示波器APP - 校准参数
    {2, &UserParamDefault.OSC_Original,                &UserParam.OSC_Original},
    {4, &UserParamDefault.OSC_Factor,                  &UserParam.OSC_Factor},
    {4, &UserParamDefault.OSC_AMP_X2,                  &UserParam.OSC_AMP_X2},
    {4, &UserParamDefault.OSC_AMP_X4,                  &UserParam.OSC_AMP_X4},
    {4, &UserParamDefault.OSC_AMP_X8,                  &UserParam.OSC_AMP_X8},
    {4, &UserParamDefault.OSC_AMP_X16,                 &UserParam.OSC_AMP_X16},
    {4, &UserParamDefault.OSC_AMP_X32,                 &UserParam.OSC_AMP_X32},
    {4, &UserParamDefault.OSC_AMP_X64,                 &UserParam.OSC_AMP_X64},
    {4, &UserParamDefault.OSC_AMP_X128,                &UserParam.OSC_AMP_X128},
    // 信号发生器APP - 校准参数
    {2, &UserParamDefault.DDS_Original,                &UserParam.DDS_Original},
    {4, &UserParamDefault.DDS_Factor,                  &UserParam.DDS_Factor},
    // 万用表APP - 电压表校准参数
    {2, &UserParamDefault.DMM_Voltage_Original,        &UserParam.DMM_Voltage_Original},
    {4, &UserParamDefault.DMM_Voltage_Factor_B,        &UserParam.DMM_Voltage_Factor_B},
    {4, &UserParamDefault.DMM_Voltage_Factor_R,        &UserParam.DMM_Voltage_Factor_R},
    // 万用表APP - 电流表校准参数
    {2, &UserParamDefault.DMM_Current_Original,        &UserParam.DMM_Current_Original},
    {4, &UserParamDefault.DMM_Current_Factor,          &UserParam.DMM_Current_Factor},
    // 万用表APP - 电阻表校准参数
    {4, &UserParamDefault.DMM_Res_Voltage,             &UserParam.DMM_Res_Voltage},
    {4, &UserParamDefault.DMM_Res_Parasitic,           &UserParam.DMM_Res_Parasitic},
    {4, &UserParamDefault.DMM_Res_R200,                &UserParam.DMM_Res_R200},
    {4, &UserParamDefault.DMM_Res_R2K,                 &UserParam.DMM_Res_R2K},
    {4, &UserParamDefault.DMM_Res_R100k,               &UserParam.DMM_Res_R100k},
    // 其他配置
    {2, &UserParamDefault.Screen_Brightness,           &UserParam.Screen_Brightness},
    {2, &UserParamDefault.Screen_Sleeptime,            &UserParam.Screen_Sleeptime},
    {2, &UserParamDefault.Screen_ShowFlip,             &UserParam.Screen_ShowFlip},
    {2, &UserParamDefault.Beezer_Volume,               &UserParam.Beezer_Volume},
    {2, &UserParamDefault.Beezer_Time,                 &UserParam.Beezer_Time},
    {2, &UserParamDefault.Fan_Enable,                  &UserParam.Fan_Enable},
    {2, &UserParamDefault.Fan_StartTemperture,         &UserParam.Fan_StartTemperture},
};

// -------------------------- 核心函数实现 --------------------------
/**
 * @brief  从配置文件加载所有参数到UserParam结构体
 * @note   加载失败时会自动重置为默认值并写入文件
 * @return HAL_OK = 加载成功，HAL_ERROR = 加载失败
 */
HAL_StatusTypeDef UserParam_LoadAllValues(void)
{
    // 第一步：先赋值默认值，防止加载失败后参数为空
    UserParam = UserParamDefault;

    // 第二步：尝试打开配置文件
    res = f_open(&fil, PARAM_PATH, FA_READ);
    if (res != FR_OK) {
        f_close(&fil);
        return UserParam_ResetToDefault(); // 打开失败则重置默认值并写入文件
    }

    // 第三步：分配缓冲区
    uint8_t *buffer = (uint8_t *)pvPortMalloc(PARAM_TOTAL_BYTES);
    if (buffer == NULL) {
        f_close(&fil);
        return HAL_ERROR;
    }

    // 第四步：读取文件内容到缓冲区
    br = 0;
    res = f_read(&fil, buffer, PARAM_TOTAL_BYTES, &br);
    f_close(&fil); // 立即关闭文件，减少资源占用

    // 第五步：校验读取结果
    if (res != FR_OK || br != PARAM_TOTAL_BYTES) {
        vPortFree(buffer); // 释放缓冲区
        return HAL_ERROR;
    }

    // 第六步：查表更新UserParam结构体
    for (uint32_t i = 0; i < Param_Number; i++) {
        uint8_t *src = &buffer[i * 4];                  // 缓冲区中当前参数的起始地址
        const ParamDesc_t *desc = &param_desc_table[i]; // 当前参数的描述信息

        // 根据参数长度拷贝数据（2字节/4字节）
        if (desc->data_len == 2) {
            // 2字节类型（int16_t/uint16_t）：仅拷贝低2字节
            memcpy(desc->current_ptr, src, 2);
        } else if (desc->data_len == 4) {
            // 4字节类型（float）：完整拷贝4字节
            memcpy(desc->current_ptr, src, 4);
        }
    }

    // 第七步：释放缓冲区并返回成功
    vPortFree(buffer);
    return HAL_OK;
}

/**
 * @brief  修改单个参数的值并写入文件
 * @param  id: 参数ID（UserParamType_e枚举）
 * @param  value: 要写入的参数值指针
 * @return HAL_OK = 修改成功，HAL_ERROR = 修改失败
 */
HAL_StatusTypeDef UserParam_UpdateSingle(UserParamType_e id, const void *value)
{
    // 第一步：校验参数ID合法性
    if (id >= Param_Number) {
        return HAL_ERROR;
    }

    // 第二步：打开配置文件
    res = f_open(&fil, PARAM_PATH, FA_OPEN_EXISTING | FA_WRITE);
    if (res != FR_OK) {
        return HAL_ERROR;
    }

    // 第三步：计算参数在文件中的偏移位置（每个参数占4字节）
    FSIZE_t offset = (FSIZE_t)id * 4UL;
    res = f_lseek(&fil, offset); // 移动文件指针到目标位置
    if (res != FR_OK) {
        f_close(&fil);
        return HAL_ERROR;
    }

    // 第四步：准备4字节写入缓冲区（默认清零）
    uint8_t buf[4] = {0};
    const ParamDesc_t *desc = &param_desc_table[id];

    // 根据参数长度拷贝值到缓冲区
    if (desc->data_len == 2) {
        memcpy(buf, value, 2); // 2字节类型：仅拷贝低2字节
    } else if (desc->data_len == 4) {
        memcpy(buf, value, 4); // 4字节类型：完整拷贝
    }

    // 第五步：写入文件并校验结果
    bw = 0;
    res = f_write(&fil, buf, 4, &bw);
    f_close(&fil); // 关闭文件

    // 第六步：返回操作结果
    if (res != FR_OK || bw != 4) {
        return HAL_ERROR;
    }
    return HAL_OK;
}

/**
 * @brief  重置配置文件为默认值（覆盖原有内容）
 * @note   内存中的UserParam保持原样，仅修改文件中的默认值
 * @return HAL_OK = 重置成功，HAL_ERROR = 写入失败
 */
HAL_StatusTypeDef UserParam_ResetToDefault(void)
{
    // 第一步：打开/创建配置文件（覆盖模式）
    res = f_open(&fil, PARAM_PATH, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        return HAL_ERROR;
    }

    // 第二步：分配缓冲区（FreeRTOS内存管理）
    uint8_t *pBuf = (uint8_t *)pvPortMalloc(PARAM_TOTAL_BYTES);
    if (pBuf == NULL) {
        f_close(&fil);
        return HAL_ERROR;
    }
    memset(pBuf, 0, PARAM_TOTAL_BYTES); // 缓冲区初始化为0

    // 第三步：查表填充默认值到缓冲区（替代原switch-case）
    for (uint32_t i = 0; i < Param_Number; i++) {
        uint8_t *pParam = &pBuf[i * 4];                // 缓冲区中当前参数的起始地址
        const ParamDesc_t *desc = &param_desc_table[i];// 当前参数的描述信息

        // 根据参数长度拷贝默认值
        if (desc->data_len == 2) {
            memcpy(pParam, desc->default_ptr, 2); // 2字节类型：仅拷贝低2字节
        } else if (desc->data_len == 4) {
            memcpy(pParam, desc->default_ptr, 4); // 4字节类型：完整拷贝
        }
    }

    // 第四步：一次性写入缓冲区到文件
    bw = 0;
    res = f_write(&fil, pBuf, PARAM_TOTAL_BYTES, &bw);

    // 第五步：释放资源并校验结果
    vPortFree(pBuf); // 释放缓冲区
    f_close(&fil);   // 关闭文件

    // 返回结果：写入字节数等于总长度且无错误则成功
    return ((res == FR_OK) && (bw == PARAM_TOTAL_BYTES)) ? HAL_OK : HAL_ERROR;
}

/**
 * @brief  将内存中UserParam所有参数写入配置文件（覆盖原有内容）
 * @note   与ResetToDefault逻辑一致，但填充的是当前参数值
 * @return HAL_OK = 写入成功，HAL_ERROR = 操作失败
 */
HAL_StatusTypeDef UserParam_SaveAllValues(void)
{
    // 第一步：打开/创建配置文件（覆盖模式）
    res = f_open(&fil, PARAM_PATH, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        return HAL_ERROR;
    }

    // 第二步：分配缓冲区（FreeRTOS内存管理）
    uint8_t *pBuf = (uint8_t *)pvPortMalloc(PARAM_TOTAL_BYTES);
    if (pBuf == NULL) {
        f_close(&fil);
        return HAL_ERROR;
    }
    memset(pBuf, 0, PARAM_TOTAL_BYTES); // 缓冲区初始化为0

    // 第三步：查表填充当前参数值到缓冲区（替代原switch-case）
    for (uint32_t i = 0; i < Param_Number; i++) {
        uint8_t *pParam = &pBuf[i * 4];                // 缓冲区中当前参数的起始地址
        const ParamDesc_t *desc = &param_desc_table[i];// 当前参数的描述信息

        // 根据参数长度拷贝当前值
        if (desc->data_len == 2) {
            memcpy(pParam, desc->current_ptr, 2); // 2字节类型：仅拷贝低2字节
        } else if (desc->data_len == 4) {
            memcpy(pParam, desc->current_ptr, 4); // 4字节类型：完整拷贝
        }
    }

    // 第四步：一次性写入缓冲区到文件
    bw = 0;
    res = f_write(&fil, pBuf, PARAM_TOTAL_BYTES, &bw);

    // 第五步：释放资源并校验结果
    vPortFree(pBuf); // 释放缓冲区
    f_close(&fil);   // 关闭文件

    // 返回结果：写入字节数等于总长度且无错误则成功
    return ((res == FR_OK) && (bw == PARAM_TOTAL_BYTES)) ? HAL_OK : HAL_ERROR;
}